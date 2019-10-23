#!/bin/bash

SCRIPT_DIR="$$( cd "$$( dirname "$$0" )" && pwd )"

$ODIN_SERVER --config=$$SCRIPT_DIR/$CONFIG --logging=error $EXTRA_PARAMS
