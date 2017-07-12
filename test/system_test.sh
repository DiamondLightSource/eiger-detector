#!/bin/bash
# System test to create the Eiger processing chain, and use a simulated Eiger to create a data and meta file
# Arguments:
# 1 - Odin-data build directory               (e.g. /dls_sw/work/tools/RHEL6-x86_64/odin/odin-data/build)
# 2 - Eiger build directory                   (e.g. /dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/build-dir)
# 3 - Eiger testdata directory                (e.g. /dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/test/testdata/)
# 4 - Meta Listener directory                 (e.g. /dls_sw/work/tools/RHEL6-x86_64/odin/eiger-daq/tools/python/)
# 5 - Directory containing a suitable VENV    (e.g. /dls_sw/work/tools/RHEL6-x86_64/odin/)
# 6 - Directory to write files to             (e.g. /tmp)

if [ "$#" -ne 6 ]; then
    echo "Error: Expecting 6 arguments"
    exit 1
fi

odinbuilddir=$1
eigerbuilddir=$2
eigertestdir=$3
metalistenerdir=$4
venvdir=$5
outputdir=$6

echo 'Removing any pre-existing files'
rm $outputdir/test_1_meta.hdf5
rm $outputdir/sys_test.hdf5

echo 'Starting Meta Listener'

cd $metalistenerdir
dls-python MetaListener.py -i tcp://127.0.0.1:5558 -d $outputdir &
metalistener_pid=$!

sleep 2

echo 'Starting Frame Receiver'

cd $odinbuilddir
./bin/frameReceiver -s Eiger --path=$eigerbuilddir/lib -m 500000000 -d 3 -p 31600 --rxtype zmq &
framereceiver_pid=$!

sleep 2

echo 'Starting Frame Processor'

cd $odinbuilddir
export HDF5_PLUGIN_PATH=/dls_sw/work/tools/RHEL6-x86_64/hdf5filters/prefix/h5plugin
./bin/frameProcessor -N --datasets data --dims 2167 2070 --dtype 1 --compression 2 --path $eigerbuilddir/lib --detector Eiger --rank 0 --output sys_test.hdf5 --output-dir $outputdir/ --acqid=test_1 --frames 3 &
frameprocessor_pid=$!

echo 'Starting EigerFan'

cd $eigerbuilddir/bin/
./eigerfan &
eigerfan_pid=$!

sleep 2

echo 'Sending Stream Data'

cd $eigertestdir
$eigerbuilddir/bin/streamSender

sleep 2

echo 'Killing EigerFan'
kill $eigerfan_pid

echo 'Killing FrameProcessor'
kill $frameprocessor_pid

echo 'Killing FrameReceiver'
kill $framereceiver_pid

echo 'Killing Meta Listener'
kill $metalistener_pid

sleep 2


alltestspassed=true

echo ''
echo '*** Checking files ***'

datasize_output=$(/dls_sw/prod/tools/RHEL6-x86_64/hdf5/1-10-1/prefix/bin/h5dump -Hp $outputdir/sys_test.hdf5 | grep DATASPACE)
datasize_test="SIMPLE { ( 3, 2167, 2070 ) /"

if [[ "$datasize_output" = *"$datasize_test"* ]]
then
    echo 'Data size matches'
else
    echo 'ERROR: Data written was different size to expected'
    echo $datasize_output' does not contain ' $datasize_test
    alltestspassed=false
fi 

datafilter_output=$(/dls_sw/prod/tools/RHEL6-x86_64/hdf5/1-10-1/prefix/bin/h5dump -Hp $outputdir/sys_test.hdf5 | grep FILTER_ID)
datafilter_test="FILTER_ID 32008"

if [[ "$datafilter_output" = *"$datafilter_test"* ]]
then
    echo 'Data filter matches'
else
    echo 'ERROR: Data filter was different to expected'
    echo $datafilter_output' does not contain ' $datafilter_test
    alltestspassed=false
fi 

fw_output=$(/dls_sw/prod/tools/RHEL6-x86_64/hdf5/1-10-1/prefix/bin/h5dump -d frame_written -p $outputdir/test_1_meta.hdf5 | grep "(0)")

fw_test="   (0): 0, 1, 2"

if [ "$fw_output" = "$fw_test" ]
then
    echo 'Frames written matches'
else
    echo 'ERROR: Frames written were different to expected'
    echo $fw_output' does not equal ' $fw_test
    alltestspassed=false
fi 
    
fr_output=$(/dls_sw/prod/tools/RHEL6-x86_64/hdf5/1-10-1/prefix/bin/h5dump -d frame -p $outputdir/test_1_meta.hdf5 | grep "(0)")
fr_test="   (0): 0, 1, 2"

if [ "$fr_output" = "$fr_test" ]
then
    echo 'Frame numbers matches'
else
    echo 'ERROR: Frames numbers were different to expected'
    echo $fr_output' does not equal ' $fr_test
    alltestspassed=false
fi 

ow_output=$(/dls_sw/prod/tools/RHEL6-x86_64/hdf5/1-10-1/prefix/bin/h5dump -d offset_written -p $outputdir/test_1_meta.hdf5 | grep "(0)")

ow_test="   (0): 0, 1, 2"

if [ "$ow_output" = "$ow_test" ]
then
    echo 'Offset written matches'
else
    echo 'ERROR: Offset written were different to expected'
    echo $ow_output ' does not equal ' $ow_test
    alltestspassed=false
fi 

cr_output=$(/dls_sw/prod/tools/RHEL6-x86_64/hdf5/1-10-1/prefix/bin/h5dump -d countrate -Hp $outputdir/test_1_meta.hdf5 | grep "DATASPACE")

cr_test="SIMPLE { ( 4000, 2 ) / ( 4000, 2 ) }"

if [[ "$cr_output" = *"$cr_test"* ]]
then
    echo 'Countrate size matches'
else
    echo 'ERROR: Countrate sizes were different to expected'
    echo $cr_output ' does not contain ' $cr_test
    alltestspassed=false
fi 

se_output=$(/dls_sw/prod/tools/RHEL6-x86_64/hdf5/1-10-1/prefix/bin/h5dump -d series -p $outputdir/test_1_meta.hdf5 | grep "(0)")

se_test="   (0): 6"

if [ "$se_output" = "$se_test" ]
then
    echo 'Series written matches'
else
    echo 'ERROR: Series written were different to expected'
    echo $se_output ' does not equal ' $se_test
    alltestspassed=false
fi 


if [ $alltestspassed == false ]
then
    echo '*** Not all tests passed ***'
    exit 1
else
    echo '*** All tests passed ***'
    exit 0
fi 
   
