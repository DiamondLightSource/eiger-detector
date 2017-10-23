#!/bin/bash
# Start the Frame Receiver 1 with rank 1 with parameters to connect to the EigerFan
cd /dls_sw/work/tools/RHEL6-x86_64/odin/odin-data/build
./bin/frameReceiver -s Eiger --path=/dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/build-dir/lib -m 16000000000 -d 3 -p 31601 --rxtype zmq -i cs04r-sc-serv-116 --sharedbuf OdinDataBuffer2 --ctrl tcp://*:6000 --ready tcp://*:6001 --release tcp://*:6002 --logconfig /dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/odinLog4cxx2.xml
