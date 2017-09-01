from pkg_resources import require
require('pyzmq')

import zmq
import json
import argparse
import os
from datetime import datetime

def options():
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--directory", default="/tmp/", help="Default directory to write meta data files to")
    args = parser.parse_args()
    return args

def main():

    args = options()

    #mh = MetaListener(args.directory, args.inputs, args.ctrl)

    #mh.run()

    context = zmq.Context()
    ctrlSocket = context.socket(zmq.REQ)
    ctrlSocket.connect("tcp://127.0.0.1:5659")
    reply = json.dumps({'msg_type':'cmd','msg_val':'configure', 'params': {'output_dir':args.directory, 'acquisition_id':'test_1'}, 'timestamp':datetime.now().isoformat()})
    ctrlSocket.send(reply)


if __name__ == "__main__":
    main()
