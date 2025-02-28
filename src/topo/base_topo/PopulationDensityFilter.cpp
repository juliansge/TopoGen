/**
 * Copyright (c) 2013-2015, Michael Grey and Markus Theil
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PopulationDensityFilter.hpp"
#include <boost/log/trivial.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "config/Config.hpp"
#include "config/PredefinedValues.hpp"
#include "db/InternetUsageStatistics.hpp"
#include "db/SQLiteAreaPopulationReader.hpp"
#include "geo/CityNode.hpp"
#include "geo/GeometricHelpers.hpp"
#include "geo/GeographicPosition.hpp"
#include "geo/SeaCableLandingPoint.hpp"
#include "topo/Graph.hpp"
#include "util/Util.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <iostream>

// Constructor
PopulationDensityFilter::PopulationDensityFilter(BaseTopology_Ptr baseTopo, int filterFlag)
    : _dbFilename(PredefinedValues::dbFilePath()), _baseTopo(baseTopo), _filterFlag(filterFlag) {
    // Load polygons if the flag is set to 1
    if (_filterFlag == 1) {
        loadPolygonsFromJSON("filter_data/mountainRanges.json");
    }
}

void PopulationDensityFilter::loadPolygonsFromJSON(const std::string& filename) {
    try {
        boost::property_tree::ptree root;
        boost::property_tree::read_json(filename, root);
        BOOST_LOG_TRIVIAL(info) << "Successfully loaded JSON file";
        for (const auto& feature : root.get_child("features")) {
            try {
                const auto& geometry = feature.second.get_child("geometry");
                std::string geomType = geometry.get<std::string>("type");
                BOOST_LOG_TRIVIAL(info) << "Processing geometry of type: " << geomType;
                if (geomType == "MultiPolygon") {
                    Polygon polygon;
                    // First level of coordinates array (MultiPolygon)
                    for (const auto& polyArray : geometry.get_child("coordinates")) {
                        // Second level (individual polygons)
                        for (const auto& ringArray : polyArray.second) {
                            std::vector<std::pair<double, double>> ring;
                            // Third level (coordinate pairs)
                            for (const auto& coordArray : ringArray.second) {
                                try {
                                    // Get the array of two values
                                    std::vector<double> coords;
                                    for (const auto& coord : coordArray.second) {
                                        coords.push_back(std::stod(coord.second.data()));
                                    }
                                    if (coords.size() == 2) {
                                        ring.push_back({ coords[0], coords[1] });
                                    }
                                }
                                catch (const std::exception& e) {
                                    BOOST_LOG_TRIVIAL(error) << "Failed to parse coordinate: " << e.what();
                                    continue;
                                }
                            }
                            if (!ring.empty()) {
                                polygon.coordinates.push_back(ring);
                            }
                        }
                    }
                    if (!polygon.coordinates.empty()) {
                        _polygons.push_back(polygon);
                        BOOST_LOG_TRIVIAL(info) << "Added polygon with " << polygon.coordinates.size() << " rings";
                    }
                }
            }
            catch (const boost::property_tree::ptree_error& e) {
                BOOST_LOG_TRIVIAL(error) << "Error processing feature: " << e.what();
                continue;
            }
        }
        BOOST_LOG_TRIVIAL(info) << "Successfully loaded " << _polygons.size() << " polygons";
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error loading JSON file: " << e.what();
        throw;
    }
}

bool PopulationDensityFilter::isPointInPolygon(const std::pair<double, double>& point, const std::vector<std::pair<double, double>>& polygon) {
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        if (((polygon[i].second > point.second) != (polygon[j].second > point.second)) &&
            (point.first < (polygon[j].first - polygon[i].first) * (point.second - polygon[i].second) / (polygon[j].second - polygon[i].second) + polygon[i].first)) {
            inside = !inside;
        }
    }
    return inside;
}

bool PopulationDensityFilter::doLineSegmentsIntersect(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4) {
    double denom = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    if (std::abs(denom) < 1e-8) return false;
    double ua = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / denom;
    double ub = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / denom;
    return ua >= 0 && ua <= 1 && ub >= 0 && ub <= 1;
}

bool PopulationDensityFilter::intersectsAnyPolygon(const GeographicPosition& p1, const GeographicPosition& p2) {
    for (const auto& polygon : _polygons) {
        for (const auto& ring : polygon.coordinates) {
            // Check intersection with polygon edges
            for (size_t i = 0; i < ring.size() - 1; i++) {
                if (doLineSegmentsIntersect(p1.lon(), p1.lat(), p2.lon(), p2.lat(), ring[i].first, ring[i].second, ring[i + 1].first, ring[i + 1].second)) {
                    return true;
                }
            }
            // Check last edge (connecting last point to first point)
            if (doLineSegmentsIntersect(p1.lon(), p1.lat(), p2.lon(), p2.lat(), ring.back().first, ring.back().second, ring.front().first, ring.front().second)) {
                return true;
            }
            // Check if midpoint is inside polygon
            auto midpoint = std::make_pair((p1.lon() + p2.lon()) / 2.0, (p1.lat() + p2.lat()) / 2.0);
            if (isPointInPolygon(midpoint, ring)) {
                return true;
            }
        }
    }
    return false;
}

void PopulationDensityFilter::filter() {
    using namespace lemon;
    std::unique_ptr<InternetUsageStatistics> inetStat(new InternetUsageStatistics(PredefinedValues::dbFilePath()));
    std::unique_ptr<Config> config(new Config);
    const double MIN_LENGTH = config->get<double>("lengthFilter.minLength");
    const double POPULATION_THRESHOLD = config->get<double>("lengthFilter.populationThreshold");
    const double BETA = config->get<double>("lengthFilter.beta");
    Graph& graph = *_baseTopo->getGraph();
    auto& nodeGeoNodeMap = *_baseTopo->getNodeMap();
    EdgeList edges_to_delete;
    auto isValidNode = [](GeographicNode_Ptr& ptr) -> bool {
        CityNode* n1 = dynamic_cast<CityNode*>(ptr.get());
        SeaCableLandingPoint* n2 = dynamic_cast<SeaCableLandingPoint*>(ptr.get());
        return n1 != nullptr || n2 != nullptr;
    };

    for (ListGraph::EdgeIt it(graph); it != INVALID; ++it) {
        Graph::Node u = graph.u(it);
        Graph::Node v = graph.v(it);
        GeographicNode_Ptr nd1 = nodeGeoNodeMap[u];
        GeographicNode_Ptr nd2 = nodeGeoNodeMap[v];
        if (isValidNode(nd1) && isValidNode(nd2)) {
            GeographicPosition p1(nd1->lat(), nd1->lon());
            GeographicPosition p2(nd2->lat(), nd2->lon());

            // Check if edge crosses any polygon only if the filter flag is set to 1
            if (_filterFlag == 1 && intersectsAnyPolygon(p1, p2)) {
                edges_to_delete.push_back(it);
                continue;
            }

            double c = GeometricHelpers::sphericalDist(p1, p2);
            double c_km = GeometricHelpers::sphericalDistToKM(c);
            if (c_km < MIN_LENGTH) {
                continue;
            }

            // INIT Bounding box reader
            GeographicPositionTuple midPoint = GeometricHelpers::getMidPointCoordinates(p1, p2);
            GeographicPosition midPointPos(midPoint.first, midPoint.second);
            SQLiteAreaPopulationReader_Ptr areaReader(new SQLiteAreaPopulationReader(_dbFilename, midPoint.first, midPoint.second, GeometricHelpers::rad2deg(c)));
            double accPopulation = 0.0;
            while (areaReader->hasNext() && accPopulation <= POPULATION_THRESHOLD) {
                PopulatedPosition next = areaReader->getNext();
                // Nothing to accumulate, skip
                assert(next._population >= 0.0);
                if (next._population == 0.0) {
                    continue;
                }

                // test if the populated position is within a more sophisticated area
                // (derived from a beta-skeleton shape parameter)
                GeographicPosition toTest(next._lat, next._lon);
                double a = GeometricHelpers::sphericalDist(p1, toTest);
                double b = GeometricHelpers::sphericalDist(p2, toTest);
                double C = Util::ihs((Util::hs(c) - Util::hs(a - b)) / (sin(a) * sin(b)));
                // Point is out of area, skip
                const double theta = M_PI - asin(BETA);
                if (C < theta) {
                    continue;
                }

                // Weight by technology factor and additional weight
                double amountInetUsers = (*inetStat)[next._country] / 100.0;
                double popWeight = 1.0 - (GeometricHelpers::sphericalDist(toTest, midPointPos) / (0.5 * c));
                // simply weight by distance to midpoint coordinate
                accPopulation += popWeight * next._population * pow(amountInetUsers, 2) * pow((MIN_LENGTH / c_km), 2);
            }
            if (accPopulation <= POPULATION_THRESHOLD) {
                edges_to_delete.push_back(it);
            }
        }
    }
    BOOST_LOG_TRIVIAL(info) << edges_to_delete.size() << " edges deleted by population density filter";
    for (EdgeList::iterator edge = edges_to_delete.begin(); edge != edges_to_delete.end(); ++edge) {
        graph.erase(*edge);
    }
}

void PopulationDensityFilter::filterByLength() {
    using namespace lemon;
    std::unique_ptr<InternetUsageStatistics> inetStat(new InternetUsageStatistics(PredefinedValues::dbFilePath()));
    std::unique_ptr<Config> config(new Config);
    const double MIN_LENGTH = config->get<double>("lengthFilter.minLength");
    Graph& graph = *_baseTopo->getGraph();
    auto& nodeGeoNodeMap = *_baseTopo->getNodeMap();
    EdgeList edges_to_delete;
    auto isValidNode = [](GeographicNode_Ptr& ptr) -> bool {
        CityNode* n1 = dynamic_cast<CityNode*>(ptr.get());
        SeaCableLandingPoint* n2 = dynamic_cast<SeaCableLandingPoint*>(ptr.get());
        return n1 != nullptr || n2 != nullptr;
    };
    auto isCityNode = [](GeographicNode_Ptr& ptr) -> bool {
        CityNode* n1 = dynamic_cast<CityNode*>(ptr.get());
        return n1 != nullptr;
    };

    for (ListGraph::EdgeIt it(graph); it != INVALID; ++it) {
        Graph::Node u = graph.u(it);
        Graph::Node v = graph.v(it);
        GeographicNode_Ptr nd1 = nodeGeoNodeMap[u];
        GeographicNode_Ptr nd2 = nodeGeoNodeMap[v];
        if (isValidNode(nd1) && isValidNode(nd2)) {
            GeographicPosition p1(nd1->lat(), nd1->lon());
            GeographicPosition p2(nd2->lat(), nd2->lon());

            // Check for intersection before calculating internet users only if the filter flag is set to 1
            if (_filterFlag == 1 && intersectsAnyPolygon(p1, p2)) {
                edges_to_delete.push_back(it);
                continue;
            }

            double amountInetUsers = 0.0;
            if (isCityNode(nd1) && isCityNode(nd2)) {
                std::string nd1c = static_cast<CityNode*>(nd1.get())->country();
                std::string nd2c = static_cast<CityNode*>(nd2.get())->country();
                amountInetUsers += (*inetStat)[nd1c] / 100.0;
                amountInetUsers += (*inetStat)[nd2c] / 100.0;
                amountInetUsers /= 2.0;
            }
            else if (isCityNode(nd1)) {
                std::string nd1c = static_cast<CityNode*>(nd1.get())->country();
                amountInetUsers += (*inetStat)[nd1c] / 100.0;
            }
            else if (isCityNode(nd2)) {
                std::string nd2c = static_cast<CityNode*>(nd2.get())->country();
                amountInetUsers += (*inetStat)[nd2c] / 100.0;
            }
            else {
                continue; // Skip edges between landing points
            }
            assert(amountInetUsers < 1.0);
            double c = GeometricHelpers::sphericalDist(p1, p2);
            double c_km = GeometricHelpers::sphericalDistToKM(c);
            if (c_km > MIN_LENGTH * (1.0 + amountInetUsers)) {
                edges_to_delete.push_back(it);
            }
        }
    }
    // Erase edges
    for (EdgeList::iterator edge = edges_to_delete.begin(); edge != edges_to_delete.end(); ++edge) {
        graph.erase(*edge);
    }
}