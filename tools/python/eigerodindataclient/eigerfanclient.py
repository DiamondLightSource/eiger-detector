import sys

from odin_data.ipc_client import IpcClient

class EigerFanClient(IpcClient):

    CTRL_PORT = 5559

    STATE = "state"
    CONSUMERS = "num_connected"
    SERIES = "series"
    FRAME = "frame"

    def __init__(self, ip_address):
        super(EigerFanClient, self).__init__(ip_address, self.CTRL_PORT)

        self.latest_frame = None
        self.current_consumers = None

    def request_status(self):
        return self.send_request("status")["params"]

    def request_latest_frame(self):
        self.update_monitors()
        return self.latest_frame

    def current_consumers(self):
        self.update_monitors()
        return self.current_consumers

    def update_monitors(self):
        status = self.request_status()
        self.latest_frame = status["frame"]
        self.current_consumers = status["num_conn"]

    def rewind(self, frames, active_frame):
        config = {
            "frames": int(frames),
            "active_frame": int(active_frame)
        }
        self.send_configuration(config, "rewind")

    def stop(self):
        config = {
            "close": True
        }
        self.send_configuration(config)

    def kill(self):
        config = {
            "kill": True,
        }
        self.send_configuration(config)
