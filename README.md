eiger-daq
=========

Data Acquisition from the Eiger detector.

eigerfan: a fan-out of the Eiger zeromq push/pull stream.

dependencies
------------



build instructions
------------------

Build out of source dir using cmake:

    mkdir build
    cd build
    cmake -DCMAKE_VERBOSE_MAKEFILE=ON \
          -DCMAKE_BUILD_TYPE=Debug \
          -DBOOST_ROOT=/dls_sw/prod/tools/RHEL6-x86_64/boost/1-48-0/prefix \
          -DZEROMQ_ROOTDIR=/dls_sw/prod/tools/RHEL6-x86_64/zeromq/3-2-4/prefix \
          -DLOG4CXX_ROOT_DIR=/dls_sw/prod/tools/RHEL6-x86_64/log4cxx/0-10-0/prefix \
          ..

    make VERBOSE=1
    
Optionally you can also do `make install` but then you may want to first tell
cmake where to install to with the `-DCMAKE_INSTALL_PREFIX=/path/to/destination`

Build as DLS Controls tool
--------------------------

The build script and configure/RELEASE definitions are already prepared for
building this module on the DLS Controls build servers as a 'tool'.

Test this on a local machine from a directory level above the eiger-daq dir:

    build_program eiger-daq
    
This will run cmake to configure the dependencies and then run `make` and
`make install`. The output will be installed into the eiger-daq/prefix dir.

