#!/bin/bash
HDF5FILTERS_PREFIX=$HOME/hdf5filters
git clone https://github.com/DiamondLightSource/hdf5filters.git
cd hdf5filters
git checkout master
mkdir -p cmake-build;
cd cmake-build;
cmake -DCMAKE_INSTALL_PREFIX=$HDF5FILTERS_PREFIX -DCMAKE_BUILD_TYPE=Release -DUSE_AVX2=ON -DBLOSC_ROOT_DIR=$HOME/c-blosc-1.14.2 -DHDF5_ROOT=$HOME/hdf5-1.10.1 ..
make -j 8 VERBOSE=1
make install
