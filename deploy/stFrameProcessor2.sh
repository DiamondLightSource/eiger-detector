#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

export HDF5_PLUGIN_PATH=/odin/h5plugin

/odin/bin/frameProcessor --ctrl=tcp://0.0.0.0:10014 --config=$SCRIPT_DIR/fp2.json --log-config $SCRIPT_DIR/log4cxx.xml
