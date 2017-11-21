#!/bin/bash
# Start the EigerFan with parameters to connect to the Eiger over the 10g network and expect 4 consumers
cd ../prefix/bin/
./eigerfan -s 172.23.102.201 -n 4 -z 4 -b 1000

