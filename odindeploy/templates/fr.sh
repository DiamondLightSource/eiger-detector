#!/bin/bash

SCRIPT_DIR="$$( cd "$$( dirname "$$0" )" && pwd )"

${NUMA}$OD_ROOT/prefix/bin/frameReceiver --sharedbuf=odin_buf_$BUFFER_IDX -m $SHARED_MEMORY --iothreads $IO_THREADS --ctrl=tcp://0.0.0.0:$CTRL_PORT --ready=tcp://127.0.0.1:$READY_PORT --release=tcp://127.0.0.1:$RELEASE_PORT --json_file=$$SCRIPT_DIR/fr$NUMBER.json --logconfig $$SCRIPT_DIR/log4cxx.xml
