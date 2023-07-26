#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

# Increase maximum fds available for ZeroMQ sockets
ulimit -n 2048

/odin/venv/bin/eiger_control --config=$SCRIPT_DIR/odin_server.cfg --logging=info --access_logging=ERROR --graylog_server graylog-log-target.diamond.ac.uk:12210 --graylog_static_fields beamline=${BEAMLINE},application_name=eiger_control,detector=Eiger16M
