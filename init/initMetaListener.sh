#!/bin/bash
# Start the MetaListener with parameters to connect to the 4 writers

export PYTHONPATH=$PYTHONPATH:/dls_sw/prod/tools/RHEL6-x86_64/h5py/2-7-1/prefix/lib/python2.7/site-packages/:../tools/python

cd ../prefix/bin/

source /dls_sw/etc/profile
./eigerMetaListener -i tcp://cs04r-sc-serv-117-10g:5558,tcp://cs04r-sc-serv-117-10g:6558,tcp://cs04r-sc-serv-118-10g:5558,tcp://cs04r-sc-serv-118-10g:6558 -b 1000

