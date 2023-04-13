from pkg_resources import require
require('pyzmq')

import zmq
import json
import os
from datetime import datetime

def main():

    context = zmq.Context()
    ctrlSocket = context.socket(zmq.DEALER)
    ctrlSocket.connect("tcp://127.0.0.1:5559")
    reply = json.dumps({'msg_type':'cmd', 'id':1, 'msg_val':'configure', 'params': {'acqid':'test_1'}, 'timestamp':datetime.now().isoformat()})
    ctrlSocket.send(reply)


if __name__ == "__main__":
    main()
