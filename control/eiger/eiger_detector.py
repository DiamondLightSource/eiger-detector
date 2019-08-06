"""
eiger_detector.py - Eiger control API for the ODIN server.

Alan Greer, DLS
"""
from pkg_resources import require
require("requests")
require("odin-control")

import requests
import json
import logging
import struct
from odin.adapters.parameter_tree import ParameterAccessor, ParameterTree

class EigerDetector(object):
    STR_API = 'api'
    STR_DETECTOR = 'detector'
    STR_MONITOR = 'monitor'
    STR_CONFIG = 'config'
    STR_STATUS = 'status'
    STR_BOARD_000 = 'board_000'
    STR_BUILDER = 'builder'

    DETECTOR_CONFIG = [
        'auto_summation',
        'beam_center_x',
        'beam_center_y',
        'bit_depth_image',
        'bit_depth_readout',
        'chi_increment',
        'chi_start',
        'compression',
        'count_time',
        'countrate_correction_applied',
        'countrate_correction_count_cutoff',
        'data_collection_date',
        'description',
        'detector_distance',
        'detector_number',
        'detector_readout_time',
        'element',
        'flatfield',
        'flatfield_correction_applied',
        'frame_time',
        'kappa_increment',
        'kappa_start',
        'nimages',
        'ntrigger',
        'number_of_excluded_pixels',
        'omega_increment',
        'omega_start',
        'phi_increment',
        'phi_start',
        'photon_energy',
        'pixel_mask',
        'pixel_mask_applied',
        'roi_mode',
        'sensor_material',
        'sensor_thickness',
        'software_version',
        'threshold_energy',
        'trigger_mode',
        'two_theta_increment',
        'two_theta_start',
        'wavelength',
        'x_pixel_size',
        'x_pixels_in_detector',
        'y_pixel_size',
        'y_pixels_in_detector'
    ]
    DETECTOR_STATUS = [
        'state',
        'error',
        'time'
    ]
    DETECTOR_BOARD_STATUS = [
        'th0_temp',
        'th0_humidity'
    ]
    DETECTOR_BUILD_STATUS = [
        'dcu_buffer_free'
    ]
    
    def __init__(self, endpoint, api_version):
        # Record the connection endpoint
        self._endpoint = endpoint
        self._api_version = api_version
        self._connected = False

        self._detector_config_uri = '{}/{}/{}/{}'.format(self.STR_DETECTOR, self.STR_API, api_version, self.STR_CONFIG)
        self._detector_status_uri = '{}/{}/{}/{}'.format(self.STR_DETECTOR, self.STR_API, api_version, self.STR_STATUS)
        self._detector_monitor_uri = '{}/{}/{}/images/next'.format(self.STR_MONITOR, self.STR_API, api_version)

        # Initialise the parameter tree structure
        param_tree = {
            self.STR_API: (lambda: 0.1, {
            }),
            self.STR_DETECTOR: {
                self.STR_API: {
                    self._api_version: {
                        self.STR_CONFIG: {
                        },
                        self.STR_STATUS: {
                            self.STR_BOARD_000: {
                            },
                            self.STR_BUILDER: {
                            }
                        }
                    }
                }
            }
        }

        # Initialise configuration parameters and populate the parameter tree
        for cfg in self.DETECTOR_CONFIG:
            setattr(self, cfg, self.read_detector_config(cfg))
            param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_CONFIG][cfg] = (lambda x=getattr(self, cfg): self.get_value(x), self.get_meta(getattr(self, cfg)))

        # Initialise status parameters and populate the parameter tree
        for status in self.DETECTOR_STATUS:
            setattr(self, status, self.read_detector_status(status))
            param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))
        for status in self.DETECTOR_BOARD_STATUS:
            setattr(self, status, self.read_detector_status('{}/{}'.format(self.STR_BOARD_000, status)))
            param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][self.STR_BOARD_000][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))
        for status in self.DETECTOR_BUILD_STATUS:
            setattr(self, status, self.read_detector_status('{}/{}'.format(self.STR_BUILDER, status)))
            param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][self.STR_BUILDER][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))

        self._params = ParameterTree(param_tree)

        # Run the status update thread

    def get(self, path):
        return self._params.get(path, with_metadata=True)

    def get_value(self, item):
        # Check if the item has a value field. If it does then return it
        if 'value' in item:
            return item['value']
        return None

    def get_meta(self, item):
        # Populate any meta data items and return the dict
        meta = {}
        if 'min' in item:
            meta['min'] = item['min']
        if 'max' in item:
            meta['max'] = item['max']
        if 'allowed_values' in item:
            meta['allowed_values'] = item['allowed_values']
        if 'unit' in item:
            meta['units'] = item['unit']
        return meta

    def read_detector_config(self, item):
        # Read a specifc detector config item from the hardware
        r = requests.get('http://{}/{}/{}'.format(self._endpoint, self._detector_config_uri, item))
        return json.loads(r.text)

    def write_detector_config(self, item, value):
        # Read a specifc detector config item from the hardware
        r = requests.put('http://{}/{}/{}'.format(self._endpoint, self._detector_config_uri, item), data=json.dumps({'value': value}))
        return json.loads(r.text)

    def read_detector_status(self, item):
        # Read a specifc detector status item from the hardware
        r = requests.get('http://{}/{}/{}'.format(self._endpoint, self._detector_status_uri, item))
        return json.loads(r.text)

    def read_detector_live_image(self):
        # Read the relevant monitor stream
        r = requests.get('http://{}/{}'.format(self._endpoint, self._detector_monitor_uri))
        tiff = r.content
        # Read the header information from the image
        logging.info("Size of raw input: {}".format(len(tiff)))
        hdr = tiff[:4]
        logging.info("{}".format(struct.unpack("<L", hdr)))
        nentries = tiff[4:6]
        logging.info("{}".format(struct.unpack("<h", nentries)))

    def shutdown(self):
        pass


def main():
    logging.basicConfig(level=logging.INFO)

    test = EigerDetector('127.0.0.1:8080', '1.6.0')
    #test = EigerDetector('i13-1-eiger01', '1.6.0')
    logging.info(test.get('detector/api/1.6.0/config/x_pixel_size'))
    logging.info(test.get('detector/api/1.6.0/config/y_pixel_size'))
    logging.info(test.get('detector/api/1.6.0/config/software_version'))
    logging.info(test.get('detector/api/1.6.0/status/state'))
    logging.info(test.get('detector/api/1.6.0/status/error'))
    logging.info(test.get('detector/api/1.6.0/status/time'))
    logging.info(test.get('detector/api/1.6.0/status/builder/dcu_buffer_free'))
    logging.info(test.get('detector/api/1.6.0/config/count_time'))

#    test.read_detector_live_image()
#    logging.info(test.write_detector_config('count_time', 250))

if __name__ == "__main__":
    main()
