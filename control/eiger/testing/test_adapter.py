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
        cls.path = 'command/initialize'


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

    def test_adapter_get_initialise(self):
        response = self.adapter.get(self.path, self.request)
        assert_true(isinstance(response.data, dict))
        assert_equal(response.status_code, 200)
        self.adapter.cleanup()

    def test_adapter_get_beamcenter(self):
        response = self.adapter.get("detector/api/1.6.0/config/beam_center_x", self.request)
        expected_response = {"writeable": True,
                             "min": -3.4028234663852886e+38,
                             "max": 3.4028234663852886e+38,
                             "value": 0.0,
                             "type": "float",
                             "units": u'pixel'}
        assert_equal(response.data, expected_response)
        assert_equal(response.status_code, 200)
        self.adapter.cleanup()
