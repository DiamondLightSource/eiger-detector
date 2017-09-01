import sys

sys.path.insert(
    0, "/dls_sw/work/tools/RHEL6-x86_64/odin/odin-data/tools/python")
from common import ZMQClient


class MetaListenerClient(ZMQClient):

    CTRL_PORT = 5659

    def __init__(self, ip_address, lock):
        super(MetaListenerClient, self).__init__(ip_address, lock, 0)

        self.acquisitions = {}
        self.writing = False

    def request_status(self):
        return self.send_request("status")["params"]

    def request_latest_frame(self, acquisition_id):
        self.update_monitors()
        return self.acquisitions[acquisition_id]["written"]

    def request_finished(self, acquisition_id):
        self.update_monitors()
        return self.acquisitions[acquisition_id]["finished"]

    def update_monitors(self):
        self.acquisitions = self.request_status()["output"]
        self.writing = any(not self.acquisitions[acq_id]["finished"]
                           for acq_id in self.acquisitions.keys())

    def configure_output_dir(self, output_dir, acquisition_id):
        config = {
            "output_dir": output_dir,
            "acquisition_id": acquisition_id
        }
        self.send_configuration_dict(**config)

    def configure_flush_rate(self, frames, acquisition_id):
        config = {
            "flush": int(frames),
            "acquisition_id": acquisition_id
        }
        self.send_configuration_dict(**config)

    def stop(self, acquisition_id):
        config = {
            "stop": True,
            "acquisition_id": acquisition_id
        }
        self.send_configuration_dict(**config)

    def kill(self):
        config = {
            "kill": True,
        }
        self.send_configuration_dict(**config)