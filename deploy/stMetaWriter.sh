#!/bin/bash

/venv/bin/eiger_meta_writer -w eiger_detector.EigerMetaWriter --sensor-shape 4362 4148 --data-endpoints tcp://odin-data-1:10008,tcp://odin-data-2:10018,tcp://odin-data-3:10028,tcp://odin-data-4:10038
