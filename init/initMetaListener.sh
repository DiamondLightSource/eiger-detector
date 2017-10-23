#!/bin/bash
# Start the MetaListener with parameters to connect to the 4 writers

export PYTHONPATH=$PYTHONPATH:/dls_sw/work/tools/RHEL6-x86_64/h5py/prefix/lib/python2.7/site-packages/:/dls_sw/work/tools/RHEL6-x86_64/odin/odin-data/tools/python:/dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/tools/python

cd /dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/tools/python/

source /dls_sw/etc/profile
dls-python eigerMetaListener.py -i tcp://cs04r-sc-serv-117-10g:5558,tcp://cs04r-sc-serv-117-10g:6558,tcp://cs04r-sc-serv-118-10g:5558,tcp://cs04r-sc-serv-118-10g:6558 -b 1000

