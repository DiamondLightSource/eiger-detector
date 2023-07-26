#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

export HDF5_PLUGIN_PATH=/dls_sw/prod/tools/RHEL7-x86_64/hdf5filters/0-7-0/prefix/hdf5_1.10/h5plugin

/odin/bin/frameProcessor --ctrl=tcp://0.0.0.0:10004 --config=$SCRIPT_DIR/fp1.json --log-config $SCRIPT_DIR/log4cxx.xml
