#!/bin/bash
VERSION="integration-test"
ODIN="odin-data-${VERSION}"
ODIN_PREFIX=$CI_PROJECT_DIR/$ODIN
git clone https://github.com/odin-detector/odin-data.git
cd odin-data
git checkout $VERSION
mkdir -p build;
cd build;
cmake -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=ON -DCMAKE_INSTALL_PREFIX=$ODIN_PREFIX -DHDF5_ROOT=$HDF5_ROOT -DBLOSC_ROOT_DIR=$BLOSC_ROOT -DLOG4CXX_ROOT_DIR=$LOG4CXX_ROOT ..
make -j 8 VERBOSE=1
make install
cd ../../