import logging
import re

from odin.adapters.adapter import (
    ApiAdapter,
    ApiAdapterResponse,
    request_types,
    response_types,
)
from tornado.escape import json_decode

from .eiger_detector import EigerDetector


class EigerAdapter(ApiAdapter):

    """An OdinControl adapter for an Eiger detector"""

    def __init__(self, **kwargs):
        logging.debug("EigerAdapter init called")
        super(EigerAdapter, self).__init__(**kwargs)
        # Compile the regular expression used to resolve paths into actions and resources
        self.path_regexp = re.compile('(.*?)/(.*)')

        # Parse the connection and api information out of the adapter options
        # and initialise the detector object
        self._endpoint = None
        self._api = None
        self._detector = None
        if 'endpoint' in self.options:
            if 'api' in self.options:
                try:
                    self._endpoint = self.options['endpoint']
                    self._api = self.options['api']
                    self._detector = EigerDetector(self._endpoint, self._api)

                except Exception as e:
                    logging.error('EigerAdapter failed to create detector object: %s', e)
                    logging.exception(e)
            else:
                logging.warning('No detector api option specified in configuration')
        else:
            logging.warning('No detector endpoint option specified in configuration')

    @request_types('application/json', "application/vnd.odin-native")
    @response_types('application/json', default='application/json')
    def get(self, path, request):
        """Handle an HTTP GET request.

        This method is the implementation of the HTTP GET handler for EigerAdapter.

        :param path: URI path of the GET request
        :param request: Tornado HTTP request object
        :return: ApiAdapterResponse object to be returned to the client
        """
        logging.debug("Eiger 'get' adapter path: %s", path)
        logging.debug("Eiger 'get' adapter request.body: %s", request.body)
        try:
            response = self._detector.get(path)[path.split("/")[-1]]
            status_code = 200
        except Exception as e:
            response = {'error': str(e)}
            logging.error("Error for path: {}".format(path))
            logging.exception(e)
            status_code = 400

        return ApiAdapterResponse(response, status_code=status_code)

    @request_types('application/json', "application/vnd.odin-native")
    @response_types('application/json', default='application/json')
    def put(self, path, request):
        """Handle an HTTP PUT request.

        This method is the implementation of the HTTP PUT handler for EigerAdapter/

        :param path: URI path of the PUT request
        :param request: Tornado HTTP request object
        :return: ApiAdapterResponse object to be returned to the client
        """
        logging.debug("Eiger 'put' adapter path: %s", path)
        logging.debug("Eiger 'put' adapter request.body: %s", request.body)
        try:
            data = json_decode(request.body)
            self._detector.set(path, data)
            response = self._detector.get(path)
            status_code = 200
        except (TypeError, ValueError) as e:
            response = {'error': 'Failed to decode PUT request body: {}'.format(str(e))}
            logging.error(e)
            status_code = 400
        except Exception as e:
            response = {'error': str(e)}
            status_code = 400
            logging.error("Error for path: {}".format(path))
            logging.exception(e)

        return ApiAdapterResponse(response, status_code=status_code)

    def cleanup(self):
        if self._detector:
            self._detector.shutdown()
