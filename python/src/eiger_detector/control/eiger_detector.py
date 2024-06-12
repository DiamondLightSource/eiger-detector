"""
eiger_detector.py - Eiger control API for the ODIN server.

Alan Greer, DLS
"""

import json
import logging
import struct
import threading
import time

import requests
import zmq
from odin.adapters.parameter_tree import ParameterAccessor, ParameterTree

from .eiger_options import option_config_options


class EigerDetector(object):
    STR_API = 'api'
    STR_DETECTOR = 'detector'
    STR_MONITOR = 'monitor'
    STR_STREAM = 'stream'
    STR_FW = 'filewriter'
    STR_CONFIG = 'config'
    STR_STATUS = 'status'
    STR_COMMAND = 'command'
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
        'counting_mode',
        'countrate_correction_applied',
        'countrate_correction_count_cutoff',
        'data_collection_date',
        'description',
        'detector_distance',
        'detector_number',
        'detector_readout_time',
        'element',
        'flatfield_correction_applied',
        'frame_time',
        'kappa_increment',
        'kappa_start',
        'nimages',
        'ntrigger',
        'omega_increment',
        'omega_start',
        'phi_increment',
        'phi_start',
        'photon_energy',
        'pixel_mask_applied',
        'roi_mode',
        'sensor_material',
        'sensor_thickness',
        'software_version',
        'threshold_energy',
        'trigger_mode',
        'trigger_start_delay',
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
        'time',
        'link_0',
        'link_1',
        'link_2',
        'link_3',
        'high_voltage/state'
    ]
    DETECTOR_BOARD_STATUS = [
        'th0_temp',
        'th0_humidity'
    ]
    DETECTOR_BUILD_STATUS = [
        'dcu_buffer_free'
    ]
    MONITOR_CONFIG = [
        'mode'
    ]
    STREAM_CONFIG = [
        'mode',
        'header_detail',
        'header_appendix',
        'image_appendix'
    ]
    STREAM_STATUS = [
        'dropped'
    ]
    FW_CONFIG = [
        'mode',
        'compression_enabled'
    ]

    TIFF_ID_IMAGEWIDTH = 256
    TIFF_ID_IMAGEHEIGHT = 257
    TIFF_ID_BITDEPTH = 258
    TIFF_ID_STRIPOFFSETS = 273
    TIFF_ID_ROWSPERSTRIP = 278

    # Parameters which trigger a (slow) re-calculation of the bit depth
    DETECTOR_BITDEPTH_TRIGGERS = ['photon_energy', 'threshold_energy'] # unused, for information
    DETECTOR_BITDEPTH_PARAM = 'bit_depth_image'

    def __init__(self, endpoint, api_version):
        # Record the connection endpoint
        self._endpoint = endpoint
        self._api_version = api_version
        self._executing = True
        self._connected = False
        self._sequence_id = 0
        self._initializing = False
        self._hv_resetting = False
        self._error = ''
        self._acquisition_complete = True
        self._armed = False
        self._live_view_enabled = False
        self._live_view_frame_number = 0

        # Re-fetch of the parameters; last fetch of certain parameters stale
        self._stale_parameters = []
        self._lock = threading.Lock()

        self.trigger_exposure = 0.0
        self.manual_trigger = False

        self._trigger_event = threading.Event()
        self._acquisition_event = threading.Event()
        self._initialize_event = threading.Event()
        self._hv_resetting_event = threading.Event()

        self._detector_config_uri = f"{self.STR_DETECTOR}/{self.STR_API}/{api_version}/{self.STR_CONFIG}"
        self._detector_status_uri = f"{self.STR_DETECTOR}/{self.STR_API}/{api_version}/{self.STR_STATUS}"
        self._detector_monitor_uri = f"{self.STR_MONITOR}/{self.STR_API}/{api_version}/images/next"
        self._detector_command_uri = f"{self.STR_DETECTOR}/{self.STR_API}/{api_version}/{self.STR_COMMAND}"
        self._stream_config_uri = f"{self.STR_STREAM}/{self.STR_API}/{api_version}/{self.STR_CONFIG}"
        self._stream_status_uri = f"{self.STR_STREAM}/{self.STR_API}/{api_version}/{self.STR_STATUS}"
        self._monitor_config_uri = f"{self.STR_MONITOR}/{self.STR_API}/{api_version}/{self.STR_CONFIG}"
        self._filewriter_config_uri = f"{self.STR_FW}/{self.STR_API}/{api_version}/{self.STR_CONFIG}"

        self.missing_parameters = []

        # Check if we need to initialize
        param = self.read_detector_status('state')
        if 'value' in param:
            if param['value'] == 'na':
                # We should re-init the detector immediately
                logging.warning("Detector found in uninitialized state at startup, initializing...")
                self.write_detector_command('initialize')

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
            },
            self.STR_STREAM: {
                self.STR_API: {
                    self._api_version: {
                        self.STR_CONFIG: {
                        },
                        self.STR_STATUS: {
                        },
                    },
                },
            },
            self.STR_MONITOR: {
                self.STR_API: {
                    self._api_version: {
                        self.STR_CONFIG: {
                        },
                    },
                },
            },
            self.STR_FW: {
                self.STR_API: {
                    self._api_version: {
                        self.STR_CONFIG: {
                        },
                    },
                },
            }
        }

        # Initialise configuration parameters and populate the parameter tree
        for cfg in self.DETECTOR_CONFIG:
            param =  self.read_detector_config(cfg)
            if param is not None:
                setattr(self, cfg, param)
                # Check if the config item is read/write
                writeable = False
                if 'access_mode' in param:
                    if param['access_mode'] == 'rw':
                        writeable = True

                if writeable is True:
                    param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_CONFIG][cfg] = (lambda x=cfg: self.get_value(getattr(self, x)),
                                                                                                            lambda value, x=cfg: self.set_value(x, value),
                                                                                                            self.get_meta(getattr(self, cfg)))
                else:
                    param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_CONFIG][cfg] = (lambda x=cfg: self.get_value(getattr(self, x)), self.get_meta(getattr(self, cfg)))
            else:
                logging.error("Parameter {} has not been implemented for API {}".format(cfg, self._api_version))
                self.missing_parameters.append(cfg)

        # Initialise status parameters and populate the parameter tree
        for status in self.DETECTOR_STATUS:
            try:
                reply = self.read_detector_status(status)
                if reply is not None:
                    # Test for special cases link_x.  These are enums but do not have the allowed values set in the hardware
                    if 'link_' in status:
                        reply['allowed_values'] = ['down', 'up']
                    setattr(self, status, reply)
                    param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))
                else:
                    logging.error("Status {} has not been implemented for API {}".format(status, self._api_version))
                    self.missing_parameters.append(status)
            except:
                # For a 500K link_2 and link_3 status will fail and return exceptions here, which is OK
                if status == 'link_2' or status == 'link_3':
                    param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][status] = (lambda: 'down', {'allowed_values': ['down', 'up']})
                else:
                    raise

        # Insert internal stale parameters flag into detector status parameters
        param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS]['stale_parameters'] = (
            self.has_stale_parameters,
            {}
        )

        for status in self.DETECTOR_BOARD_STATUS:
            reply = self.read_detector_status('{}/{}'.format(self.STR_BOARD_000, status))
            if reply is not None:
                setattr(self, status, reply)
                param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][self.STR_BOARD_000][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))
            else:
                logging.error("Status {} has not been implemented for API {}".format(status, self._api_version))
                self.missing_parameters.append(status)

        for status in self.DETECTOR_BUILD_STATUS:
            reply = self.read_detector_status('{}/{}'.format(self.STR_BUILDER, status))
            if reply is not None:
                setattr(self, status, reply)
                param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][self.STR_BUILDER][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))
            else:
                logging.error("Status {} has not been implemented for API {}".format(status, self._api_version))
                self.missing_parameters.append(status)

        for status in self.STREAM_STATUS:
            reply = self.read_stream_status(status)
            if reply is not None:
                setattr(self, status, reply)
                param_tree[self.STR_STREAM][self.STR_API][self._api_version][self.STR_STATUS][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))
            else:
                logging.error("Status {} has not been implemented for API {}".format(status, self._api_version))
                self.missing_parameters.append(status)

        # Initialise stream config items
        for cfg in self.STREAM_CONFIG:
            if cfg == 'mode':
                self.stream_mode = self.read_stream_config('mode')
                param_tree[self.STR_STREAM][self.STR_API][self._api_version][self.STR_CONFIG]['mode'] = (lambda x='stream_mode': self.get_value(getattr(self, x)),
                                                                                                         lambda value: self.set_mode(self.STR_STREAM, value),
                                                                                                         self.get_meta(self.stream_mode))

            else:
                setattr(self, cfg, self.read_stream_config(cfg))
                param_tree[self.STR_STREAM][self.STR_API][self._api_version][self.STR_CONFIG][cfg] = (lambda x=cfg: self.get_value(getattr(self, x)),
                                                                                                    lambda value, x=cfg: self.set_value(x, value),
                                                                                                    self.get_meta(getattr(self, cfg)))

#param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_STATUS][status] = (lambda x=getattr(self, status): self.get_value(x), self.get_meta(getattr(self, status)))

        # Initialise monitor mode
        self.monitor_mode = self.read_monitor_config('mode')
        param_tree[self.STR_MONITOR][self.STR_API][self._api_version][self.STR_CONFIG]['mode'] = (lambda x='monitor_mode': self.get_value(getattr(self, x)),
                                                                                                  lambda value: self.set_mode(self.STR_MONITOR, value),
                                                                                                  self.get_meta(self.monitor_mode))

        # Initialise filewriter config items
        for cfg in self.FW_CONFIG:
            if cfg == 'mode':
                # Initialise filewriter mode
                self.fw_mode = self.read_filewriter_config('mode')
                param_tree[self.STR_FW][self.STR_API][self._api_version][self.STR_CONFIG]['mode'] = (lambda x='fw_mode': self.get_value(getattr(self, x)),
                                                                                                    lambda value: self.set_mode(self.STR_FW, value),
                                                                                                    self.get_meta(self.fw_mode))
            else:
                setattr(self, cfg, self.read_filewriter_config(cfg))
                param_tree[self.STR_FW][self.STR_API][self._api_version][self.STR_CONFIG][cfg] = (lambda x=cfg: self.get_value(getattr(self, x)),
                                                                                                  lambda value, x=cfg: self.set_value(x, value),
                                                                                                  self.get_meta(getattr(self, cfg)))


        # Initialise additional ADOdin configuration items
        if self._api_version != '1.8.0':
            param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_CONFIG]['ccc_cutoff'] = (lambda: self.get_value(self.countrate_correction_count_cutoff), self.get_meta(self.countrate_correction_count_cutoff))
        param_tree['status'] = {
            'manufacturer': (lambda: 'Dectris', {}),
            'model': (lambda: 'Odin [Eiger {}]'.format(self._api_version), {}),
            'state': (self.get_state, {}),
            'sensor': {
                'width': (lambda: self.get_value(self.x_pixels_in_detector), self.get_meta(self.x_pixels_in_detector)),
                'height': (lambda: self.get_value(self.y_pixels_in_detector), self.get_meta(self.y_pixels_in_detector)),
                'bytes': (lambda: self.get_value(self.x_pixels_in_detector) *
                         self.get_value(self.y_pixels_in_detector) *
                         self.get_value(self.bit_depth_image) / 8, {})
            },
            'sequence_id': (lambda: self._sequence_id, {}),
            'error': (lambda: self._error, {}),
            'acquisition_complete': (lambda: self._acquisition_complete, {}),
            'armed': (lambda: self._armed, {})
        }
        param_tree['config'] = {
            'trigger_exposure': (lambda: self.trigger_exposure,
                                 lambda value: setattr(self, 'trigger_exposure', value),
                                 {}),
            'manual_trigger': (lambda: self.manual_trigger,
                               lambda value: setattr(self, 'manual_trigger', value),
                               {}),
            'num_images': (lambda: self.get_value(self.nimages),
                              lambda value: self.set_value('nimages', value),
                              self.get_meta(self.nimages)),
            'exposure_time': (lambda: self.get_value(self.count_time),
                              lambda value: self.set_value('count_time', value),
                              self.get_meta(self.count_time)),
            'live_view': (lambda: self._live_view_enabled,
                          lambda value: setattr(self, '_live_view_enabled', value),
                          {})
        }
        param_tree[self.STR_DETECTOR][self.STR_API][self._api_version][self.STR_COMMAND] = {
            'initialize': (lambda: 0,
                           lambda value: self.write_detector_command('initialize')),
            'arm': (lambda: 0, lambda value: self.write_detector_command('arm')),
            'trigger': (lambda: 0,
                        lambda value: self.write_detector_command('trigger')),
            'disarm': (lambda: 0, lambda value: self.write_detector_command('disarm')),
            'cancel': (lambda: 0, lambda value: self.write_detector_command('cancel')),
            'abort': (lambda: 0, lambda value: self.write_detector_command('abort')),
            'wait': (lambda: 0, lambda value: self.write_detector_command('wait'))
        }

        self._params = ParameterTree(param_tree)


        self._lv_context = zmq.Context()
        self._lv_publisher = self._lv_context.socket(zmq.PUB)
        self._lv_publisher.bind("tcp://*:5555")

        # Run the live view update thread
        self._lv_thread = threading.Thread(target=self.lv_loop)
        self._lv_thread.start()

        # Run the acquisition thread
        self._acq_thread = threading.Thread(target=self.do_acquisition)
        self._acq_thread.start()

        # Run the initialize thread
        self._init_thread = threading.Thread(target=self.do_initialize)
        self._init_thread.start()

        # Run the initialize thread
        self._status_thread = threading.Thread(target=self.do_check_status)
        self._status_thread.start()

        # Run the hv resetting thread
        self._hv_reset_thread = threading.Thread(target=self.do_hv_reset)
        self._hv_reset_thread.start()

    def read_all_config(self):
        for cfg in self.DETECTOR_CONFIG:
            param =  self.read_detector_config(cfg)
            setattr(self, cfg, param)

    def get_state(self):
        odin_states = {
            'idle': 0,
            'acquire': 1
        }
        # Get the detector state and map to an ADOdin state
        if 'value' in self.state:
            if self.state['value'] in odin_states:
                return odin_states[self.state['value']]
        return 0

    def get(self, path):
        # Check for ODIN specific commands
        if path == 'command/start_acquisition':
            return {'value': 0}
        elif path == 'command/stop_acquisition':
            return {'value': 0}
        elif path == 'command/send_trigger':
            return {'send_trigger': {'value': 0}}
        elif path == 'command/initialize':
            return {'initialize': {'value': self._initializing}}
        elif path == 'command/hv_reset':
            return {'hv_reset': {'value': self._hv_resetting}}
        else:
            return self._params.get(path, with_metadata=True)

    def set(self, path, value):
        # Check for ODIN specific commands
        if path == 'command/start_acquisition':
            return self.start_acquisition()
        elif path == 'command/stop_acquisition':
            return self.stop_acquisition()
        elif path == 'command/send_trigger':
            return self.send_trigger()
        elif path == 'command/initialize':
            return self.initialize_detector()
        elif path == 'command/hv_reset':
            return self.hv_reset_detector()
        else:
            # mbbi record will send integers; change to string
            if any(option == path.split("/")[-1] for option in option_config_options):
                value = str(value)
            return self._params.set(path, value)

    def get_value(self, item):
        # Check if the item has a value field. If it does then return it
        if 'value' in item:
            return item['value']
        return None

    def set_mode(self, mode_type, value):
        logging.info("Setting {} mode to {}".format(mode_type, value))
        # Intercept integer values and convert to string values where
        # option not index is expected
        value = option_config_options["mode"].get_option(value)
        if mode_type == self.STR_STREAM:
            response = self.write_stream_config('mode', value)
            param = self.read_stream_config('mode')
            self.stream_mode = param
        elif mode_type == self.STR_MONITOR:
            response = self.write_monitor_config('mode', value)
            param = self.read_monitor_config('mode')
            self.monitor_mode = param
        elif mode_type == self.STR_FW:
            response = self.write_filewriter_config('mode', value)
            param = self.read_filewriter_config('mode')
            self.fw_mode = param

    def set_value(self, item, value):
        response = None
        logging.info("Setting {} to {}".format(item, value))
        # Intercept integer values and convert to string values where
        # option not index is expected
        if any(option == item for option in option_config_options):
            value = option_config_options[item].get_option(value)
        # First write the value to the hardware
        if item in self.DETECTOR_CONFIG:
            response = self.write_detector_config(item, value)
        elif item in self.STREAM_CONFIG:
            response = self.write_stream_config(item, value)
        elif item in self.FW_CONFIG:
            response = self.write_filewriter_config(item, value)

        # Now check the response to see if we need to update any config items
        if response is not None:
            if isinstance(response, list):
                with self._lock:
                    self._stale_parameters += response
                    logging.debug(
                        "Stale parameters after put: %s", self._stale_parameters
                    )
        else:
            if item in self.DETECTOR_CONFIG:
                param = self.read_detector_config(item)
            elif item in self.STREAM_CONFIG:
                param = self.read_stream_config(item)
            elif item in self.FW_CONFIG:
                param = self.read_filewriter_config(item)
            elif item in self.DETECTOR_STATUS:
                param = self.read_detector_status(item)
            logging.info("Read from detector [{}]: {}".format(item, param))
            setattr(self, item, param)

    def parse_response(self, response, item):
        reply = None
        try:
            reply = json.loads(response.text)
        except:
            # If parameter unavailable, do not repeat logging
            for missing in self.missing_parameters:
                if missing in item:
                    return None
            # Unable to parse the json response, so simply log this
            logging.error("Failed to parse a JSON response: {}".format(response.text))
        return reply

    def get_meta(self, item):
        # Populate any meta data items and return the dict
        meta = {}
        for field in ['min', 'max', 'allowed_values']:
            if field in item:
                meta[field] = item[field]
        if 'unit' in item:
            meta['units'] = item['unit']
        return meta

    def read_detector_config(self, item):
        # Read a specifc detector config item from the hardware
        r = requests.get(f"http://{self._endpoint}/{self._detector_config_uri}/{item}")
        parsed_reply = self.parse_response(r, item)
        parsed_reply = self.intercept_reply(item, parsed_reply)
        return parsed_reply

    def write_detector_config(self, item, value):
        # Read a specifc detector config item from the hardware
        r = requests.put(f"http://{self._endpoint}/{self._detector_config_uri}/{item}",
                         data=json.dumps({'value': value}),
                         headers={"Content-Type": "application/json"})
        return self.parse_response(r, item)

    def read_detector_status(self, item):
        # Read a specifc detector status item from the hardware
        r = requests.get(f"http://{self._endpoint}/{self._detector_status_uri}/{item}")
        return self.parse_response(r, item)

    def write_detector_command(self, command, value=None):
        # Write a detector specific command to the detector
        reply = None
        data = None
        if value is not None:
            data=json.dumps({'value': value})
        r = requests.put(f"http://{self._endpoint}/{self._detector_command_uri}/{command}",
                         data=data,
                         headers={"Content-Type": "application/json"})
        if len(r.text) > 0:
            reply = self.parse_response(r, command)
        return reply

    def read_stream_config(self, item):
        # Read a specifc detector config item from the hardware
        r = requests.get(f"http://{self._endpoint}/{self._stream_config_uri}/{item}")
        parsed_reply = self.parse_response(r, item)
        parsed_reply = self.intercept_reply(item, parsed_reply)
        return parsed_reply

    def write_stream_config(self, item, value):
        # Read a specifc detector config item from the hardware
        r = requests.put(f"http://{self._endpoint}/{self._stream_config_uri}/{item}",
                         data=json.dumps({'value': value}),
                         headers={"Content-Type": "application/json"})
        return self.parse_response(r, item)

    def read_stream_status(self, item):
        # Read a specifc stream status item from the hardware
        r = requests.get(f"http://{self._endpoint}/{self._stream_status_uri}/{item}")
        return self.parse_response(r, item)

    def read_monitor_config(self, item):
        # Read a specifc monitor config item from the hardware
        r = requests.get(f"http://{self._endpoint}/{self._monitor_config_uri}/{item}")
        parsed_reply = self.parse_response(r, item)
        parsed_reply = self.intercept_reply(item, parsed_reply)
        return parsed_reply

    def write_monitor_config(self, item, value):
        # Read a specifc detector config item from the hardware
        r = requests.put(f"http://{self._endpoint}/{self._monitor_config_uri}/{item}",
                         data=json.dumps({'value': value}),
                         headers={"Content-Type": "application/json"})
        return self.parse_response(r, item)

    def read_filewriter_config(self, item):
        # Read a specifc filewriter config item from the hardware
        r = requests.get(f"http://{self._endpoint}/{self._filewriter_config_uri}/{item}")
        parsed_reply = self.parse_response(r, item)
        parsed_reply = self.intercept_reply(item, parsed_reply)
        return parsed_reply

    def write_filewriter_config(self, item, value):
        # Write a specifc filewriter config item to the hardware
        r = requests.put(f"http://{self._endpoint}/{self._filewriter_config_uri}/{item}",
                         data=json.dumps({'value': value}),
                         headers={"Content-Type": "application/json"})
        return self.parse_response(r, item)

    def read_detector_live_image(self):
        # Read the relevant monitor stream
        r = requests.get(f"http://{self._endpoint}/{self._detector_monitor_uri}")
        if r.status_code != 200:
            logging.debug("No image")
            # There is no live image (1.6.0 or 1.8.0) so we can just pass through
            return
        else:
            tiff = r.content
            # Read the header information from the image
            logging.debug("Size of raw stream input: {}".format(len(tiff)))
            hdr = tiff[4:8]
            index_offset = struct.unpack("=i", hdr)[0]
            hdr = tiff[index_offset:index_offset+2]
            logging.debug("Number of tags: {}".format(struct.unpack("=h", hdr)[0]))
            number_of_tags = struct.unpack("=h", hdr)[0]

            image_width = -1
            image_height = -1
            image_bitdepth = -1
            image_rows_per_strip = -1
            image_strip_offset = -1

            for tag_index in range(number_of_tags):
                tag_offset = index_offset+2+(tag_index*12)
                logging.debug("Tag number {}: entry offset: {}".format(tag_index, tag_offset))
                hdr = tiff[tag_offset:tag_offset+2]
                tag_id = struct.unpack("=h", hdr)[0]
                hdr = tiff[tag_offset+2:tag_offset+4]
                tag_data_type = struct.unpack("=h", hdr)[0]
                hdr = tiff[tag_offset+4:tag_offset+8]
                tag_data_count = struct.unpack("=i", hdr)[0]
                hdr = tiff[tag_offset+8:tag_offset+12]
                tag_data_offset = struct.unpack("=i", hdr)[0]

                logging.debug("   Tag ID: {}".format(tag_id))
                logging.debug("   Tag Data Type: {}".format(tag_data_type))
                logging.debug("   Tag Data Count: {}".format(tag_data_count))
                logging.debug("   Tag Data Offset: {}".format(tag_data_offset))

                # Now check for the width, hieght, bitdepth, rows per strip, and strip offsets
                if tag_id == self.TIFF_ID_IMAGEWIDTH:
                    image_width = tag_data_offset
                elif tag_id == self.TIFF_ID_IMAGEHEIGHT:
                    image_height = tag_data_offset
                elif tag_id == self.TIFF_ID_BITDEPTH:
                    image_bitdepth = tag_data_offset
                elif tag_id == self.TIFF_ID_ROWSPERSTRIP:
                    image_rows_per_strip = tag_data_offset
                elif tag_id == self.TIFF_ID_STRIPOFFSETS:
                    image_strip_offset = tag_data_offset

            if image_width > -1 and image_height > -1 and image_bitdepth > -1 and image_rows_per_strip == image_height and image_strip_offset > -1:
                # We have a valid image so construct the required object and publish it
                frame_header = {
                    'frame_num': self._live_view_frame_number,
                    'acquisition_id': '',
                    'dtype': 'uint{}'.format(image_bitdepth),
                    'dsize': image_bitdepth/8,
                    'dataset': 'data',
                    'compression': 0,
                    'shape': ["{}".format(image_height), "{}".format(image_width)]
                }
                logging.info("Frame object created: {}".format(frame_header))

                frame_data = tiff[image_strip_offset:image_strip_offset+(image_width * image_height * image_bitdepth / 8)]

                self._lv_publisher.send_json(frame_header, flags=zmq.SNDMORE)
                self._lv_publisher.send(frame_data, 0)
                self._live_view_frame_number += 1

    def intercept_reply(self, item, reply):
        # Intercept detector config for options where we convert to index for
        # unamabiguous definition and update config to allow these
        if any(option == item for option in option_config_options):
            # Inconsitency over mapping of index to string
            # communication via integer, uniquely converted to mapping as defined in eiger_options
            value = reply[u'value']
            reply[u'value'] = option_config_options[item].get_index(value)
            reply[u'allowed_values'] = option_config_options[item].get_allowed_values()
        return reply

    def arm_detector(self):
        # Write a detector specific command to the detector
        logging.info("Arming the detector")
        s_obj = self.write_detector_command('arm')
        # We are looking for the sequence ID
        self._sequence_id = s_obj['sequence id']
        logging.info("Arm complete, returned sequence ID: {}".format(self._sequence_id))

    def initialize_detector(self):
        self._initializing = True
        # Write a detector specific command to the detector
        logging.info("Initializing the detector")
        self._initialize_event.set()

    def hv_reset_detector(self):
        self._hv_resetting = True
        logging.info("HV Resetting the detector")
        self._hv_resetting_event.set()

    def fetch_stale_parameters(self, blocking=False):
        with self._lock:
            # Take a copy in case it is changed while we fetch
            previous_stale_parameters = list(self._stale_parameters)

        # Iterate the parameters and fetch (only fetch duplicates once)
        for cfg in set(previous_stale_parameters):
            if cfg in self.DETECTOR_CONFIG:
                param = self.read_detector_config(cfg)
                logging.info("Read from detector config [{}]: {}".format(cfg, param))
                setattr(self, cfg, param)
            elif cfg in self.STREAM_CONFIG:
                param = self.read_stream_config(cfg)
                logging.info("Read from stream config [{}]: {}".format(cfg, param))
                setattr(self, cfg, param)
            else:
                logging.warning("Did not fetch [{}] as not in parameter lists".format(cfg))


        with self._lock:
            # Remove only the parameters we have just fetched from the list
            # This will leave any that were added while fetching in place
            remaining_stale_parameters = []
            for param in self._stale_parameters:
                if param in previous_stale_parameters:
                    previous_stale_parameters.remove(param)
                else:
                    remaining_stale_parameters.append(param)

            self._stale_parameters[:] = remaining_stale_parameters

            logging.debug(
                "Stale parameters after fetch: %s", self._stale_parameters
            )

        # _stale_parameters may have been repopulated again by the time we get here
        if blocking and self._stale_parameters:
            logging.info("Stale parameters repopulated - fetching again")
            self.fetch_stale_parameters(blocking)

    def has_stale_parameters(self):
        with self._lock:
            return len(self._stale_parameters) != 0

    def get_trigger_mode(self):
        trigger_idx = self.get_value(self.trigger_mode)
        return option_config_options['trigger_mode'].get_option(trigger_idx)

    def start_acquisition(self):
        # Perform the start sequence
        logging.info("Start acquisition called")

        # Fetch stale parameters
        self.fetch_stale_parameters(blocking=True)

        # Set the acquisition complete to false
        self._acquisition_complete = False

        # Check the trigger mode
        trigger_mode = self.get_trigger_mode()
        logging.info("trigger_mode: {}".format(trigger_mode))
        if trigger_mode == "inte" or trigger_mode == "exte":
            self.set('{}/nimages'.format(self._detector_config_uri), 1)

        # Now arm the detector
        self.arm_detector()

        # Start the acquisition thread
        if trigger_mode == "ints" or trigger_mode == "inte":
            self._acquisition_event.set()

        # Set the detector armed state to true
        self._armed = True

    def do_acquisition(self):
        while self._executing:
            if self._acquisition_event.wait(0.5):
                # Clear the acquisition event
                self._acquisition_event.clear()

                # Set the number of triggers to zero
                triggers = 0
                # Clear the trigger event
                self._trigger_event.clear()
                while self._acquisition_complete == False and triggers < self.get_value(self.ntrigger):

                    do_trigger = True

                    if self.manual_trigger:
                        do_trigger = self._trigger_event.wait(0.1)

                    if do_trigger:

                        # Send the trigger to the detector
                        trigger_mode = self.get_trigger_mode()
                        logging.info("Sending trigger to the detector {}".format(trigger_mode))

                        if trigger_mode == "inte":
                            self.write_detector_command('trigger', self.trigger_exposure)
                            time.sleep(self.trigger_exposure)
                        else:
                            self.write_detector_command('trigger')

                        # Increment the trigger count
                        triggers += 1
                        # Clear the trigger event
                        self._trigger_event.clear()

                self._acquisition_complete = True
                self.write_detector_command('disarm')
                self._armed = False

    def do_initialize(self):
        while self._executing:
            if self._initialize_event.wait(1.0):
                self.write_detector_command('initialize')
                # We are looking for the sequence ID
                logging.info("Initializing complete")
                self._initializing = False
                self._initialize_event.clear()
                self.read_all_config()

    def do_check_status(self):
        while self._executing:
            for status in self.DETECTOR_STATUS:
                if status not in self.missing_parameters:
                    try:
                        if status == 'link_2' or status == 'link_3':
                            if '500K' not in self.get_value(self.description):
                                setattr(self, status, self.read_detector_status(status))
                        else:
                            setattr(self, status, self.read_detector_status(status))
                    except:
                        pass
            # Update bit depth if it needs updating
            if self._acquisition_complete:
                try:
                    self.fetch_stale_parameters()
                except:
                    pass
            for status in self.DETECTOR_BOARD_STATUS:
                try:
                    setattr(self, status, self.read_detector_status('{}/{}'.format(self.STR_BOARD_000, status)))
                except:
                    pass
            for status in self.DETECTOR_BUILD_STATUS:
                try:
                    setattr(self, status, self.read_detector_status('{}/{}'.format(self.STR_BUILDER, status)))
                except:
                    pass
            time.sleep(.5)

    def do_hv_reset(self):
        while self._executing:
            if self._hv_resetting_event.wait(1.0):
                self.hv_reset()
                self._hv_resetting = False
                self._hv_resetting_event.clear()

    def stop_acquisition(self):
        # Perform an abort sequence
        logging.info("Stop acquisition called")
        self.write_detector_command('disarm')
        self._acquisition_complete = True
        self._armed = False

    def send_trigger(self):
        # Send a manual trigger
        logging.info("Initiating a manual trigger")
        self._trigger_event.set()

    def lv_loop(self):
        while self._executing:
            if self._live_view_enabled:
                self.read_detector_live_image()
            time.sleep(0.1)

    def _get_hv_state(self) -> str:
        hv_state = getattr(self, "high_voltage/state")["value"]

        return hv_state

    def hv_reset(self):
        logging.info("Initiating HV Reset")

        material = self.sensor_material["value"]
        # Make sure sensor material is CdTe
        if material.lower() != "cdte":
            logging.error("Sensor material is not CdTe.")
            return

        # send HV reset command, 45 seconds is recommended, in a loop of 10
        niterations = 10
        for i in range(niterations):
            counter = 0
            while self._get_hv_state() != "READY":
                counter += 1
                if counter > 60:
                    logging.error(
                        "Detector failed to be ready after 600 seconds, "
                        "Stopping hv reset"
                    )
                    return
                time.sleep(10)
            logging.info(f"Starting HV Reset iteration {i}")
            self.write_detector_command("hv_reset", 45)
            # Need to wait for command to be processed
            time.sleep(60)
        
        logging.info("HV Reset complete")

    def shutdown(self):
        self._executing = False


def main():
    logging.basicConfig(level=logging.INFO)

    test = EigerDetector('127.0.0.1:8080', '1.6.0')
    #test = EigerDetector('i13-1-eiger01', '1.6.0')
#    logging.info(test.get('detector/api/1.6.0/config/x_pixel_size'))
#    logging.info(test.get('detector/api/1.6.0/config/y_pixel_size'))
#    logging.info(test.get('detector/api/1.6.0/config/software_version'))
#    logging.info(test.get('detector/api/1.6.0/status/state'))
#    logging.info(test.get('detector/api/1.6.0/status/error'))
#    logging.info(test.get('detector/api/1.6.0/status/time'))
#    logging.info(test.get('detector/api/1.6.0/status/builder/dcu_buffer_free'))
#    logging.info(test.get('detector/api/1.6.0/config/count_time'))

    test.read_detector_live_image()
#    logging.info(test.write_detector_config('count_time', 250))

if __name__ == "__main__":
    main()
