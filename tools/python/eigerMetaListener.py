import argparse

from pkg_resources import require
require('pygelf==0.2.11')
require("h5py==2.7.0")
require('pyzmq')

from MetaListener import MetaListener
from odin_data.logconfig import setup_logging, add_graylog_handler, add_logger

def options():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--inputs", default="tcp://127.0.0.1:5558", help="Input enpoints - comma separated list")
    parser.add_argument("-d", "--directory", default="/tmp/", help="Default directory to write meta data files to")
    parser.add_argument("-c", "--ctrl", default="5659", help="Control channel port to listen on")
    args = parser.parse_args()
    return args

def main():

    args = options()

    add_graylog_handler("cs04r-sc-serv-14.diamond.ac.uk", 12202)
    add_logger("meta_listener", {"level": "DEBUG", "propagate": True})
    setup_logging()

    mh = MetaListener(args.directory, args.inputs, args.ctrl)

    mh.run()

if __name__ == "__main__":
    main()