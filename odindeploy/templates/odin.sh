#!/bin/bash

DETECTOR="${EIGER_ROOT}"
ODIN_DATA="${ODIN_DATA_ROOT}"

SCRIPT_DIR="$$( cd "$$( dirname "$$0" )" && pwd )"

Fan="$${SCRIPT_DIR}/stEigerFan.sh"
${ODIN_DATA_SCRIPTS}
MetaListener="export PYTHONPATH=$${DETECTOR}/prefix/lib/python2.7/site-packages:$${ODIN_DATA}/prefix/lib/python2.7/site-packages &&
              $${SCRIPT_DIR}/stEigerMetaListener.sh"
OdinServer="export PYTHONPATH=$${DETECTOR}/prefix/lib/python2.7/site-packages:$${ODIN_DATA}/prefix/lib/python2.7/site-packages &&
            $${SCRIPT_DIR}/stOdinServer.sh"

gnome-terminal --tab --title="Fan" -- bash -c "$${Fan}"
${ODIN_DATA_COMMANDS}
gnome-terminal --tab --title="MetaListener" -- bash -c "$${MetaListener}"
gnome-terminal --tab --title="OdinServer" -- bash -c "$${OdinServer}"
