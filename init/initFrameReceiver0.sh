#!/bin/bash
# Start the Frame Receiver 0 with rank 0 with parameters to connect to the EigerFan
cd /dls_sw/work/tools/RHEL6-x86_64/odin/odin-data/build
./bin/frameReceiver -s Eiger --path=/dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/build-dir/lib -m 16000000000 -d 3 -p 31600 --rxtype zmq -i cs04r-sc-serv-116 --sharedbuf OdinDataBuffer1 --ctrl tcp://*:5000 --ready tcp://*:5001 --release tcp://*:5002 --logconfig /dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/odinLog4cxx1.xml
