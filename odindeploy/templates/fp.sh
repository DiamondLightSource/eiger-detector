#!/bin/bash

SCRIPT_DIR="$$( cd "$$( dirname "$$0" )" && pwd )"

export HDF5_PLUGIN_PATH=${HDF5_FILTERS}

${NUMA}$OD_ROOT/prefix/bin/frameProcessor --ctrl=tcp://0.0.0.0:$CTRL_PORT --ready=tcp://127.0.0.1:$READY_PORT --release=tcp://127.0.0.1:$RELEASE_PORT --json_file=$$SCRIPT_DIR/fp$NUMBER.json --logconfig $$SCRIPT_DIR/log4cxx.xml
