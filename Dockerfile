FROM ubuntu:24.04

RUN  apt update &&  \
    # Install dependencies
    apt install -y cmake build-essential libboost-all-dev libgmp-dev libjsoncpp-dev libsqlite3-dev &&  \
    # Install Lemon \
    apt-get install -y wget && \
    rm -rf /var/lib/apt/lists/* && \
    mkdir ~/external_libs &&  \
    cd ~/external_libs &&  \
    wget http://lemon.cs.elte.hu/pub/sources/lemon-1.3.1.tar.gz &&  \
    tar xzf lemon-1.3.1.tar.gz &&  \
    cd lemon-1.3.1 &&  \
    mkdir build && cd build &&  \
    cmake .. &&  \
    make && \
    make install && \
    cd .. && \
    # CGAL dependencies \
    apt update &&  \
    apt-get install software-properties-common -y && \
    add-apt-repository universe && \
    apt-get update && \
    apt-get -y install libcgal-dev libcgal-qt5-dev


WORKDIR /app
