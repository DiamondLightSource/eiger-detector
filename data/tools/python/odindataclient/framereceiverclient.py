from odin_data.ipc_client import IpcClient


class FrameReceiverClient(IpcClient):

    CTRL_PORT = 5000
    FAN_ENDPOINT = "tcp://{}:5559"

    def __init__(self, ip_address, server_rank=0):
        port = self.CTRL_PORT + server_rank * 1000
        super(FrameReceiverClient, self).__init__(ip_address, port)
        self.free_buffers = 0
        self.frames_dropped = 0

    def request_status(self):
        response = self.send_request("status")
        return response["params"]

    def update_monitors(self):
        status = self.request_status()
        self.free_buffers = status["buffers"]["empty"]
        self.frames_dropped = status["frames"]["dropped"]

    def configure_shared_memory(self, shared_memory, ready, release):
        config = {
            "fr_shared_mem": shared_memory,
            "fr_ready_cnxn": ready,
            "fr_release_cnxn": release
        }

    def kill(self):
        response = self.send_request("shutdown")
        return response["params"]
        # self.send_configuration(self.SHARED_MEMORY_SETUP, config)
