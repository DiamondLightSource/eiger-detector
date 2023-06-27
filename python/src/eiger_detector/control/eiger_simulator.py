"""
eiger_simulator.py - Eiger simulator adapter for an ODIN server.

Alan Greer, Diamond Light Source
"""

import logging
import json
import time
from http.server import BaseHTTPRequestHandler,HTTPServer

PORT_NUMBER = 8080

class EigerSimulator(BaseHTTPRequestHandler, object):
    """EigerSimulator class.

    This class provides a simple Eiger simulator
    """
    PARAMS = {
        '/detector/api/1.6.0/config/auto_summation': {"max": 2147483647, "access_mode": "rw", "value_type": "bool", "value": True, "min": -2147483648},
        '/detector/api/1.6.0/config/beam_center_x': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "pixel"},
        '/detector/api/1.6.0/config/beam_center_y': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "pixel"},
        '/detector/api/1.6.0/config/bit_depth_image': {"max": 2147483647, "access_mode": "r", "value_type": "int", "value": 32, "min": -2147483648},
        '/detector/api/1.6.0/config/bit_depth_readout': {"max": 2147483647, "access_mode": "r", "value_type": "int", "value": 12, "min": -2147483648},
        '/detector/api/1.6.0/config/chi_increment': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/chi_start': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/compression': {"access_mode": "rw", "value_type": "string", "value": "bslz4", "allowed_values": ["lz4", "bslz4"]},
        '/detector/api/1.6.0/config/count_time': {"min": 2.9999000616953708e-06, "max": 1800, "value": 0.099895000457763672, "value_type": "float", "access_mode": "rw", "unit": "s"},
        '/detector/api/1.6.0/config/countrate_correction_applied': {"max": 2147483647, "access_mode": "rw", "value_type": "bool", "value": True, "min": -2147483648},
        '/detector/api/1.6.0/config/countrate_correction_count_cutoff': {"max": 4294967295, "access_mode": "r", "value_type": "uint", "value": 184755, "min": 0},
        '/detector/api/1.6.0/config/data_collection_date': {"access_mode": "rw", "value_type": "string", "value": "2010-13-13 12:21"},
        '/detector/api/1.6.0/config/description': {"access_mode": "r", "value_type": "string", "value": "Dectris Eiger 500K"},
        '/detector/api/1.6.0/config/detector_distance': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "m"},
        '/detector/api/1.6.0/config/detector_number': {"access_mode": "r", "value_type": "string", "value": "E-01-0165"},
        '/detector/api/1.6.0/config/detector_readout_time': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 7.4642066465457901e-06, "value_type": "float", "access_mode": "r", "unit": "s"},
        '/detector/api/1.6.0/config/element': {"access_mode": "rw", "value_type": "string", "value": "Cu", "allowed_values": ["Ru", "Ni", "Nb", "Rb", "Rh", "Ba", "Fe", "Br", "Sr", "Sn", "Sb", "Se", "Co", "Ce", "Cd", "Ge", "Ga", "Cs", "Cr", "Cu", "La", "Pd", "Xe", "Te", "Tc", "I", "Mn", "Y", "Mo", "Zn", "Ag", "Kr", "As", "In", "Zr"]},
        '/detector/api/1.6.0/config/flatfield': {"max": 3.4028234663852886e+38, "access_mode": "rw", "value_type": "float", "value": {"data": "FSGlPiFEwj7r1 ...........THERE WAS MORE DATA HERE............ h2D5uRcM+3omePg==", "shape": [514, 1030], "type": "<f4", "__darray__": [1, 0, 0], "filters": ["base64"]}, "min": -3.4028234663852886e+38},
        '/detector/api/1.6.0/config/flatfield_correction_applied': {"max": 2147483647, "access_mode": "rw", "value_type": "bool", "value": True, "min": -2147483648},
        '/detector/api/1.6.0/config/frame_time': {"min": 0.00011011111200787127, "max": 360000, "value": 0.099995002150535583, "value_type": "float", "access_mode": "rw", "unit": "s"},
        '/detector/api/1.6.0/config/kappa_increment': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/kappa_start': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/nimages': {"max": 2000000000.0, "access_mode": "rw", "value_type": "uint", "value": 1, "min": 1},
        '/detector/api/1.6.0/config/ntrigger': {"max": 2000000000.0, "access_mode": "rw", "value_type": "uint", "value": 289, "min": 1},
        '/detector/api/1.6.0/config/number_of_excluded_pixels': {"max": 4294967295, "access_mode": "r", "value_type": "uint", "value": 8, "min": 0},
        '/detector/api/1.6.0/config/omega_increment': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/omega_start': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/phi_increment': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/phi_start': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/photon_energy': {"min": 5000.0, "max": 36000.0, "value": 8047.77978515625, "value_type": "float", "access_mode": "rw", "unit": "eV"},
        '/detector/api/1.6.0/config/pixel_mask': {"max": 4294967295, "access_mode": "rw", "value_type": "uint", "value": {"data": "AAAAAAAAAAAAAAAAAAAAA ..... THERE WAS MORE DATA HERE ......... AAAAAAAAAAAAAAAAAAAAAAAAAA==", "shape": [514, 1030], "type": "<u4", "__darray__": [1, 0, 0], "filters": ["base64"]}, "min": 0},
        '/detector/api/1.6.0/config/pixel_mask_applied': {"max": 2147483647, "access_mode": "rw", "value_type": "bool", "value": True, "min": -2147483648},
        '/detector/api/1.6.0/config/roi_mode': {"access_mode": "rw", "value_type": "string", "value": "disabled", "allowed_values": "disabled"},
        '/detector/api/1.6.0/config/sensor_material': {"access_mode": "r", "value_type": "string", "value": "Si"},
        '/detector/api/1.6.0/config/sensor_thickness': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.00044999999227002263, "value_type": "float", "access_mode": "r", "unit": "m"},
        '/detector/api/1.6.0/config/software_version': {"access_mode": "r", "value_type": "string", "value": "1.6.6"},
        '/detector/api/1.6.0/config/threshold_energy': {"min": 2700.0, "max": 18000.0, "value": 4023.889892578125, "value_type": "float", "access_mode": "rw", "unit": "eV"},
        '/detector/api/1.6.0/config/trigger_mode': {"access_mode": "rw", "value_type": "string", "value": "exts", "allowed_values": ["ints", "exts", "inte", "exte"]},
        '/detector/api/1.6.0/config/two_theta_increment': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/two_theta_start': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 0.0, "value_type": "float", "access_mode": "rw", "unit": "degree"},
        '/detector/api/1.6.0/config/wavelength': {"min": 0.34440052509307861, "max": 2.4796838760375977, "value": 1.5406012535095215, "value_type": "float", "access_mode": "rw", "unit": "angstrom"},
        '/detector/api/1.6.0/config/x_pixel_size': {"min": -3.4028234663852886e+38, "max": 3.4028234663852886e+38, "value": 7.5000003562308848e-05, "value_type": "float", "access_mode": "r", "unit": "m"},
        '/detector/api/1.6.0/config/x_pixels_in_detector': {"max": 4294967295, "access_mode": "r", "value_type": "uint", "value": 1030, "min": 0},
        '/detector/api/1.6.0/config/y_pixel_size': {"min": -3.3028234663852886e+38, "max": 3.3028234663852886e+38, "value": 7.6000003562308848e-05, "value_type": "float", "access_mode": "r", "unit": "m"},
        '/detector/api/1.6.0/config/y_pixels_in_detector': {"max": 4294967295, "access_mode": "r", "value_type": "uint", "value": 514, "min": 0},
        '/detector/api/1.6.0/status/state': {"value": "idle", "state": "normal", "value_type": "string", "critical_limits": [None, None], "time": "2019-07-29T18:24:34.755835", "unit": "", "critical_values": ["na", "error"]},
        '/detector/api/1.6.0/status/error': {"value_type": "string", "value": [], "time": "2019-07-30T14:18:57+01:00"},
        '/detector/api/1.6.0/status/time': {"value_type": "string", "value": "2019-07-30T14:18:57+01:00"},
        '/detector/api/1.6.0/status/board_000/th0_temp': {"value": 24.879453125000005, "state": "normal", "value_type": "float", "critical_limits": [15.0, 37.5], "time": "2019-07-30T14:18:56.831952", "unit": "", "critical_values": []},
        '/detector/api/1.6.0/status/board_000/th0_humidity': {"value": 3.552001953125, "state": "normal", "value_type": "float", "critical_limits": [0.0, 35], "time": "2019-07-30T14:18:56.831958", "unit": "", "critical_values": []},
        '/detector/api/1.6.0/status/builder/dcu_buffer_free': {"value": 100.0, "state": "normal", "value_type": "float", "critical_limits": [2.0, None], "time": "2019-07-30T14:18:56.831456", "unit": "", "critical_values": []},
        '/detector/api/1.6.0/status/link_0': {"value": "up", "state": "normal", "value_type": "string", "critical_limits": [None, None], "time": "2019-07-30T14:18:56.831456", "unit": "", "critical_values": []},
        '/detector/api/1.6.0/status/link_1': {"value": "up", "state": "normal", "value_type": "string", "critical_limits": [None, None], "time": "2019-07-30T14:18:56.831456", "unit": "", "critical_values": []},
        '/monitor/api/1.6.0/config/mode':  {"access_mode": "rw", "value_type": "string", "value": "enabled", "allowed_values": ["enabled", "disabled"]},
        '/stream/api/1.6.0/config/mode':  {"access_mode": "rw", "value_type": "string", "value": "enabled", "allowed_values": ["enabled", "disabled"]},
        '/stream/api/1.6.0/config/header_detail':  {"access_mode": "rw", "value_type": "string", "value": "basic", "allowed_values": ["all", "basic", "none"]},
        '/stream/api/1.6.0/config/header_appendix':  {"access_mode": "rw", "value_type": "string", "value": ""},
        '/stream/api/1.6.0/config/image_appendix':  {"access_mode": "rw", "value_type": "string", "value": ""},
        '/stream/api/1.6.0/config/format': {"access_mode":"rw","allowed_values":["legacy","cbor"],"value":"legacy","value_type":"string"},
        '/stream/api/1.6.0/status/dropped': {"access_mode": "r", "value_type": "int", "value": 17},
        '/filewriter/api/1.6.0/config/mode':  {"access_mode": "rw", "value_type": "string", "value": "enabled", "allowed_values": ["enabled", "disabled"]},
        '/filewriter/api/1.6.0/config/compression_enabled':  {"access_mode": "rw", "value_type": "bool", "value": True}
    }

    LIVE_IMAGE = None
    SEQUENCE_ID = 1

    def __init__(self, *args, **kwargs):
        with open('test.tif', 'rb') as f:
            EigerSimulator.LIVE_IMAGE = f.read()
        super(EigerSimulator, self).__init__(*args, **kwargs)

    def do_GET(self):
        print(self.path)
        self.send_response(200)
        if self.path == '/monitor/api/1.6.0/images/next':
            self.send_header('Content-type','application/tiff')
            self.end_headers()
            self.wfile.write(EigerSimulator.LIVE_IMAGE)
        else:
            self.send_header('Content-type','application/json')
            self.end_headers()
            # Send the html message
            print(json.dumps(EigerSimulator.PARAMS[self.path]))
            self.wfile.write(json.dumps(EigerSimulator.PARAMS[self.path]).encode())
        return

    def do_PUT(self):
        print(self.path)
        # Check for commands
        path_arrary = self.path.split('/')
        if path_arrary[-2] == 'command':
            self.execute_command(path_arrary[-1])
        else:
            content_length = int(self.headers['Content-Length'])
            body = None
            if content_length > 0:
                body = json.loads(self.rfile.read(content_length))
                print(body)
            changed_items = ['compression', 'x_pixel_size']
            if self.path in EigerSimulator.PARAMS:
                EigerSimulator.PARAMS[self.path]['value'] = body['value']
                changed_items.append(self.path.split('/')[-1])
            self.send_response(200)
            self.send_header('Content-type','application/json')
            self.end_headers()
            # Send the html message
            self.wfile.write(json.dumps(changed_items).encode())
        return

    def execute_command(self, command):
        print("Executing {}...".format(command))
        response = {}
        if command == 'initialize':
            time.sleep(10.0)
        elif command == 'arm':
            response['sequence id'] = EigerSimulator.SEQUENCE_ID
            EigerSimulator.SEQUENCE_ID += 1
        self.send_response(200)
        self.send_header('Content-type','application/json')
        self.end_headers()
        # Send the html message
        self.wfile.write(json.dumps(response))

def main():
    try:
        #Create a web server and define the handler to manage the
        #incoming request
        server = HTTPServer(('', PORT_NUMBER), EigerSimulator)
        print(f'Started httpserver on port {PORT_NUMBER}')

        #Wait forever for incoming http requests
        server.serve_forever()

    except KeyboardInterrupt:
        print('^C received, shutting down the web server')
        server.socket.close()


if __name__ == "__main__":
    main()