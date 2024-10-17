#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

export HDF5_PLUGIN_PATH=/odin/h5plugin

/odin/bin/frameProcessor --ctrl=tcp://0.0.0.0:10024 --config=$SCRIPT_DIR/fp3.json --log-config $SCRIPT_DIR/log4cxx.xml
