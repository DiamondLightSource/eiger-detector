import argparse

from pkg_resources import require
require('pygelf==0.3.1')
require("h5py==2.8.0")
require('pyzmq==16.0.2')
require('odin-data==0-4-0dls1')

from MetaListener import MetaListener
from odin_data.logconfig import setup_logging, add_graylog_handler, add_logger

def options():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--inputs", default="tcp://127.0.0.1:5558", help="Input enpoints - comma separated list")
    parser.add_argument("-d", "--directory", default="/tmp/", help="Default directory to write meta data files to")
    parser.add_argument("-c", "--ctrl", default="5659", help="Control channel port to listen on")
    parser.add_argument("-b", "--blocksize", default=1, help="Block size within the data files")
    parser.add_argument("--logserver", default="graylog2.diamond.ac.uk:12210", help="logserver address and :port")
    parser.add_argument("--staticlogfields", default=None, help="comma separated list of key=value fields to be attached to every log message")
    args = parser.parse_args()
    return args

def main():

    args = options()

    logserver, logserverport = args.logserver.split(':')
    logserverport = int(logserverport)

    static_fields = None
    if args.staticlogfields is not None:
        static_fields = dict([f.split('=') for f in args.staticlogfields.replace(' ','').split(',')])

    add_graylog_handler(logserver, logserverport, static_fields=static_fields)
    add_logger("meta_listener", {"level": "INFO", "propagate": True})
    setup_logging()

    mh = MetaListener(args.directory, args.inputs, args.ctrl, args.blocksize)

    mh.run()

if __name__ == "__main__":
    main()
