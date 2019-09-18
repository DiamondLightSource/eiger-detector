#!/bin/bash

cd $OD_ROOT
export PYTHONPATH=$EIGER_DETECTOR_PATH/data/tools/python/
${NUMA}prefix/bin/meta_writer -w metalistener.eiger_meta_writer.EigerMetaWriter -i $IP_LIST
