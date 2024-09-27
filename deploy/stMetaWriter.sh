#!/bin/bash

/venv/bin/eiger_meta_writer -w eiger_detector.EigerMetaWriter --sensor-shape 4362 4148 --data-endpoints tcp://127.0.0.1:10008,tcp://127.0.0.1:10018,tcp://127.0.0.1:10028,tcp://127.0.0.1:10038
