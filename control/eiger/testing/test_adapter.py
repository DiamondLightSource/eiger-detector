"""
test_adapter.py - test cases for the EigerAdapter API adapter class for the ODIN server

"""

import sys
import json
from nose.tools import *

if sys.version_info[0] == 3:  # pragma: no cover
    from unittest.mock import Mock
else:                         # pragma: no cover
    from mock import Mock

from eiger.eiger_adapter import EigerAdapter

class EigerAdapterFixture(object):

    @classmethod
    def setup_class(cls, **adapter_params):
        cls.adapter = EigerAdapter(**adapter_params)
        cls.request = Mock()
        cls.request.headers = {'Accept': 'application/json', 'Content-Type': 'application/json'}


class TestEigerAdapter(EigerAdapterFixture):

    @classmethod
    def setup_class(cls):

        adapter_params = {
            'endpoint': '127.0.0.1:8080',
            'api': '1.6.0'
        }
        super(TestEigerAdapter, cls).setup_class(**adapter_params)

    def test_adapter_name(self):
        assert_equal(self.adapter.name, 'EigerAdapter')

    def test_adapter_initialise(self):
        response = self.adapter.get('command/initialize', self.request)
        assert_true(isinstance(response.data, dict))
        assert_equal(response.status_code, 200)
        self.adapter.cleanup()

    def test_adapter_get_beamcenter(self):
        response = self.adapter.get("detector/api/1.6.0/config/beam_center_x",
                                    self.request)
        expected_response = {"writeable": True,
                             "min": -3.4028234663852886e+38,
                             "max": 3.4028234663852886e+38,
                             "value": 0.0,
                             "type": "float",
                             "units": "pixel"}
        assert_equal(response.data, expected_response)
        assert_equal(response.status_code, 200)
        self.adapter.cleanup()

    def test_adapter_get_trigger_mode(self):
        response = self.adapter.get("detector/api/1.6.0/config/trigger_mode",
                                    self.request)
        expected_response = {"writeable": True,
                             "allowed_values": ['0','1','2','3'],
                             "value": '2',
                             "type": "str"}
        assert_equal(response.data, expected_response)
        assert_equal(response.status_code, 200)
        self.adapter.cleanup()

    def test_adapter_put_trigger_mode(self):

        self.request.body = json.dumps(0)
        response = self.adapter.put("detector/api/1.6.0/config/trigger_mode",
                                    self.request)
        assert_equal(response.status_code, 200)

    def test_adapter_bad_path(self):
        response = self.adapter.put('bad_path', self.request)
        assert_equal(response.status_code, 400)

    def test_adapter_put_beamcenter(self):
        self.request.body = json.dumps(10)
        response = self.adapter.put("detector/api/1.6.0/config/beam_center_x",
                                    self.request)
        assert_equal(response.status_code, 200)

    def test_adapter_delete(self):
        expected_response = 'DELETE method not implemented by {}'.\
                format(self.adapter.name)
        response = self.adapter.delete("detector/api/1.6.0/config/beam_center_x",
                                       self.request)
        assert_equal(response.data, expected_response)
        assert_equal(response.status_code, 400)

