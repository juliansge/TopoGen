/*
 * Copyright (c) 2013-2015, Michael Grey and Markus Theil
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef POPULATIONDENSITYFILTER_HPP
#define POPULATIONDENSITYFILTER_HPP

#include "topo/base_topo/BaseTopology.hpp"
#include <string>
#include <vector>
#include <utility>
#include <memory>

// Structure to represent a polygon
struct Polygon {
    std::vector<std::vector<std::pair<double, double>>> coordinates;
};

class PopulationDensityFilter;
typedef std::shared_ptr<PopulationDensityFilter> PopulationDensityFilter_Ptr;

class PopulationDensityFilter {
public:
    // Constructor with a default filterFlag value
    
    /**********************************************************************
    SET FILTER FLAG TO 0 FOR NOW MOUNTAIN RANGE FILTERING OR 1 FOR MOUNTAIN RANGE FILTERING
    ***********************************************************************/
    PopulationDensityFilter(BaseTopology_Ptr baseTopo, int filterFlag = 0);

    // somewhat complex filter algorithm involving bounding box reader and population estimation
    void filter();

    // simple filter algorithm, removing edges by weighted max-length
    void filterByLength();

protected:
private:
    std::string _dbFilename;
    BaseTopology_Ptr _baseTopo;
    int _filterFlag; // Flag to determine which filter to apply
    std::vector<Polygon> _polygons;

    // Helper function to load polygons from JSON
    void loadPolygonsFromJSON(const std::string& filename);

    // Helper function to check if a point is inside a polygon
    bool isPointInPolygon(const std::pair<double, double>& point, const std::vector<std::pair<double, double>>& polygon);

    // Helper function to check if line segments intersect
    bool doLineSegmentsIntersect(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);

    // Helper function to check if a line intersects with any polygon
    bool intersectsAnyPolygon(const GeographicPosition& p1, const GeographicPosition& p2);
};

#endif