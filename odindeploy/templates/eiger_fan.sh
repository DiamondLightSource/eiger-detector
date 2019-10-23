#!/bin/bash

SCRIPT_DIR="$$( cd "$$( dirname "$$0" )" && pwd )"

cd $EIGER_DETECTOR_PATH
${NUMA}prefix/bin/eigerfan -s $IP -n $PROCESSES -z $SOCKETS -b $BLOCK_SIZE -t $THREADS --logconfig $$SCRIPT_DIR/log4cxx.xml
