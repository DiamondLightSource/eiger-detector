#!/bin/bash
# Start the FrameProcessor with the given server_rank

server_rank=$1
port_base=$((5+${server_rank}))
buffer_index=$((1+${server_rank}))

export HDF5_PLUGIN_PATH=/dls_sw/work/tools/RHEL6-x86_64/hdf5filters/prefix/h5plugin
cd /dls_sw/work/tools/RHEL6-x86_64/odin/odin-data/build
bin/frameProcessor --ctrl tcp://*:${port_base}004 -I --ready tcp://127.0.0.1:${port_base}001 --release tcp://127.0.0.1:${port_base}002 --meta tcp://*:${port_base}558 --logconfig /dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/odinLog4cxx.xml

