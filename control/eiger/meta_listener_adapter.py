import logging

from tornado import escape

from odin.adapters.adapter import ApiAdapterResponse, \
    request_types, response_types
from odin_data.odin_data_adapter import OdinDataAdapter


class MetaListenerAdapter(OdinDataAdapter):

    """An OdinControl adapter for a MetaListener"""

    def __init__(self, **kwargs):
        logging.debug("MetaListenerAdapter init called")

        # These are internal adapter parameters
        self.acquisitionID = ""
        self.acquisition_active = False
        self.acquisitions = []
        # These parameters are stored under an acquisition tree, so we need to
        # parse out the parameters for the acquisition we have stored
        self._acquisition_parameters = {
            "status/filename": "",
            "status/num_processors": "",
            "status/writing": "",
            "status/written": "",
            "config/output_dir": "",
            "config/flush": ""
        }

        # Parameters must be created before base init called
        super(MetaListenerAdapter, self).__init__(**kwargs)
        self._client = self._clients[0]  # We only have one client

    @request_types('application/json')
    @response_types('application/json', default='application/json')
    def get(self, path, request):

        """Implementation of the HTTP GET verb for MetaListenerAdapter

        :param path: URI path of the GET request
        :param request: Tornado HTTP request object
        :return: ApiAdapterResponse object to be returned to the client

        """
        status_code = 200
        response = {}
        logging.debug("GET path: %s", path)
        logging.debug("GET request: %s", request)

        if path == "config/acquisition_id":
            response["value"] = self.acquisitionID
        elif path == "config/acquisitions":
            response["value"] = "," .join(self.acquisitions)
        elif path in self._acquisition_parameters:
            response["value"] = None
            if self.acquisitionID:
                full_path = path.replace(
                    "/", "/acquisitions/{}/".format(self.acquisitionID), 1)
                response["value"] = self.traverse_parameters(
                    self._client.parameters, full_path.split("/"))
        else:
            return super(MetaListenerAdapter, self).get(path, request)

        return ApiAdapterResponse(response, status_code=status_code)

    @request_types('application/json')
    @response_types('application/json', default='application/json')
    def put(self, path, request):

        """
        Implementation of the HTTP PUT verb for MetaListenerAdapter

        :param path: URI path of the PUT request
        :param request: Tornado HTTP request object
        :return: ApiAdapterResponse object to be returned to the client

        """
        status_code = 200
        response = {}
        logging.error("PUT path: %s", path)
        logging.error("PUT request: %s", request)
        logging.error("PUT request.body: %s",
                      str(escape.url_unescape(request.body)))

        value = str(escape.url_unescape(request.body)).replace('"', '')

        if path == "config/acquisition_id":
            self.acquisitionID = value
            # Send the PUT request
            try:
                config = {
                    "acquisition_id": self.acquisitionID,
                }
                self._client.send_configuration(config)
            except Exception as err:
                logging.debug(OdinDataAdapter.ERROR_FAILED_TO_SEND)
                logging.error("Error: %s", err)
                status_code = 503
                response = {'error': OdinDataAdapter.ERROR_FAILED_TO_SEND}
        elif path == "config/stop":
            # By default we stop all acquisitions by passing None
            config = {
                "acquisition_id": None,
                "stop": True
            }
            if self.acquisitionID:
                # If we have an Acquisition ID then stop that one only
                config["acquisition_id"] = self.acquisitionID
            try:
                self._client.send_configuration(config)
            except Exception as err:
                logging.debug(OdinDataAdapter.ERROR_FAILED_TO_SEND)
                logging.error("Error: %s", err)
                status_code = 503
                response = {'error': OdinDataAdapter.ERROR_FAILED_TO_SEND}
        elif path in self._acquisition_parameters:
            # Update the stored parameter on the expected path
            self._acquisition_parameters[path] = value
            # Send the PUT request on with the acquisitionID attached
            try:
                parameter = path.split("/")[-1]
                config = {
                    "acquisition_id": self.acquisitionID,
                    parameter: value
                }
                self._client.send_configuration(config)
            except Exception as err:
                logging.debug(OdinDataAdapter.ERROR_FAILED_TO_SEND)
                logging.error("Error: %s", err)
                status_code = 503
                response = {'error': OdinDataAdapter.ERROR_FAILED_TO_SEND}
        else:
            return super(OdinDataAdapter, self).put(path, request)

        return ApiAdapterResponse(response, status_code=status_code)

    def process_updates(self):
        """Handle additional background update loop tasks

        Reset acquisitionID if it finishes writing

        """
        if self.acquisitionID:
            currently_writing = self.traverse_parameters(
                self._clients[0].parameters,
                ["status", "acquisitions", self.acquisitionID, "writing"]
            )
            if self.acquisition_active and not currently_writing:
                self.acquisitionID = ""
            self.acquisition_active = currently_writing
