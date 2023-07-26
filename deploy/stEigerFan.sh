#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

/odin/bin/eigerfan -s 127.0.0.1 -n 4 -z 4 -b 1000 -t 2 --logconfig $SCRIPT_DIR/log4cxx.xml
