#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

/odin/bin/frameReceiver --io-threads 2 --ctrl=tcp://0.0.0.0:10020 --config=$SCRIPT_DIR/fr3.json --log-config $SCRIPT_DIR/log4cxx.xml
