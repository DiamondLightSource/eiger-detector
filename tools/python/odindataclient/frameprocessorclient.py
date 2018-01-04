import os
import ConfigParser

from odin_data.ipc_client import IpcClient

CFG_FILE = "frameprocessorclient.cfg"
CFG_PATHS_SECTION = "Plugin Paths"
CFG_ODIN_DATA_DIR = "odin-data_path"
CFG_EIGER_DIR = "eiger-detector_path"

class FrameProcessorClient(IpcClient):

    CTRL_PORT = 5004
    META_PORT = 5558
    META_ENDPOINT = "tcp://*:{}"

    PLUGIN = "plugin"
    FILE_WRITER = "hdf"
    SHARED_MEMORY_SETUP = "fr_setup"
    META = "meta_endpoint"
    INPUT = "frame_receiver"
    EIGER = "eiger"
    TIMEOUT = 6000
    FRAMES_PER_BLOCK = 1000
    BLOCKS_PER_FILE = 1

    def __init__(self, rank, processes, ip_address, server_rank=0):

        # Get the location of the odin-data and eiger-detector plugins from the config file
        this_file_path = os.path.abspath(__file__)
        cfg_file_path = os.path.join(os.path.dirname(this_file_path), CFG_FILE)
        config = ConfigParser.ConfigParser()
        config.read(cfg_file_path)
        
        self.ODIN_DATA_DIR = config.get(CFG_PATHS_SECTION, CFG_ODIN_DATA_DIR)     
        self.LIBRARIES = {self.EIGER: config.get(CFG_PATHS_SECTION, CFG_EIGER_DIR)}
        
        port = self.CTRL_PORT + server_rank * 1000
        super(FrameProcessorClient, self).__init__(ip_address, port)
        self.processes = processes
        self.rank = rank

        self.meta_endpoint = self.META_ENDPOINT.format(self.META_PORT + 1000 * server_rank)

        self.plugins = []
        self.writing = False
        self.datasets = {}
        self.frames_max = None
        self.frames_written = None
        self.current_acquisition = None

    def request_status(self):
        response = self.send_request("status")
        return response["params"]

    def request_frames_written(self):
        self.update_monitors()
        return self.frames_written

    def request_frames_expected(self):
        self.update_monitors()
        return self.frames_max

    def request_plugins(self):
        self.update_monitors()
        return self.plugins

    def update_monitors(self):
        status = self.request_status()
        self.plugins = status["plugins"]["names"]

        if self.FILE_WRITER in self.plugins:
            fw_status = status[self.FILE_WRITER]
            self.writing = fw_status["writing"]
            # self.datasets = fw_status["datasets"]
            self.frames_max = fw_status["frames_max"]
            self.frames_written = fw_status["frames_written"]
            self.current_acquisition = fw_status["acquisition_id"]
        else:
            self.writing = False
            self.datasets = {}
            self.frames_max = 0
            self.frames_written = 0
            self.current_acquisition = ""

    def stop(self):
        config = {
            "write": False
        }
        self.send_configuration(config, self.FILE_WRITER)

    def timeout(self):
        config = {
            "start_timeout_timer": True
        }
        self.send_configuration(config, self.FILE_WRITER)

    def kill(self):
        config = {
            "shutdown": True
        }
        self.send_configuration(config)

    def initialise(self, connections):
        for plugin in connections.keys():
            self.load_plugin(plugin)
        for sink, source in connections.items():
            self.connect_plugins(source, sink)

        status = self.request_status()
        if self.FILE_WRITER not in status.keys():
            raise RuntimeError("Cannot initialise unless hdf plugin is loaded")

        hdf_status = status[self.FILE_WRITER]
        if ("processes" not in hdf_status.keys()
                or hdf_status["processes"] != self.processes) \
                or ("rank" not in hdf_status.keys()
                    or hdf_status["rank"] != self.rank):
            if hdf_status["writing"]:
                raise RuntimeError(
                    "Cannot initialise while writing")
            self.configure_file_process()
            self.configure_timeout()

    def configure_shared_memory(self, shared_memory, ready, release):
        config = {
            "fr_shared_mem": shared_memory,
            "fr_ready_cnxn": ready,
            "fr_release_cnxn": release
        }
        self.send_configuration(config, self.SHARED_MEMORY_SETUP)

    def configure_meta(self):
        self.send_configuration(self.meta_endpoint, self.META)

    def load_plugin(self, plugin):
        if plugin == self.FILE_WRITER:
            self.load_file_writer_plugin(plugin)
        else:
            library, name = self.parse_plugin_definition(
                plugin, self.LIBRARIES[plugin])
            self.request_status()
            config = {
                "load": {
                    "library": library,
                    "index": plugin,
                    "name": name,
                }
            }
            self.send_configuration(
                config, self.PLUGIN,
                valid_error="Cannot load plugin with index = {},"
                            " already loaded".format(plugin))

    def load_file_writer_plugin(self, index):
        config = {
            "load": {
                "library": os.path.join(self.ODIN_DATA_DIR, "libHdf5Plugin.so"),
                "index": index,
                "name": "FileWriterPlugin",
            },
        }
        self.send_configuration(config, self.PLUGIN,
                                valid_error="Cannot load plugin with index = "
                                            "{}, already loaded".format(index))

    def connect_plugins(self, source, sink):
        config = {
            "connect": {
                "connection": source,
                "index": sink
            }
        }
        self.send_configuration(config, self.PLUGIN)

    def disconnect_plugins(self, source, sink):
        config = {
            "disconnect": {
                "connection": source,
                "index": sink
            }
        }
        self.send_configuration(config, self.PLUGIN)

    def configure_file(self, path, name, frames, acq_id=None):
        config = {
            "file": {
                "path": path,
                "name": name,
            },
            "frames": int(frames),
            "write": True
        }
        if acq_id is not None:
            config["acquisition_id"] = acq_id
        self.send_configuration(config, self.FILE_WRITER, timeout=3000)

    def create_dataset(self, name, dtype, dimensions,
                       chunks=None, compression=None):
        config = {
            "dataset": {
                "cmd": "create",
                "name": name,
                "datatype": int(dtype),
                "dims": dimensions
            }
        }
        if chunks is not None:
            config["dataset"]["chunks"] = chunks
        if compression is not None:
            config["dataset"]["compression"] = int(compression)
        self.send_configuration(config, self.FILE_WRITER, timeout=3000)

    def configure_file_process(self):
        config = {
            "process": {
                "number": int(self.processes),
                "rank": int(self.rank),
                "frames_per_block": int(self.FRAMES_PER_BLOCK),
                "blocks_per_file": int(self.BLOCKS_PER_FILE),
                "earliest_version": True,
            },
        }
        self.send_configuration(config, self.FILE_WRITER)

    def configure_timeout(self):
        config = {
            "timeout_timer_period": self.TIMEOUT 
        }
        self.send_configuration(config, self.FILE_WRITER)

    def configure_frame_count(self, frames):
        config = {
            "frames": frames
        }
        self.send_configuration(config, self.FILE_WRITER)

    def rewind(self, frames, active_frame):
        config = {
            "rewind": int(frames),
            "rewind/active_frame": int(active_frame)
        }
        self.send_configuration(config, self.FILE_WRITER)

    def set_initial_frame(self, frame):
        config = {
            "initial_frame": int(frame)
        }
        self.send_configuration(config, self.FILE_WRITER)

    @staticmethod
    def parse_plugin_definition(name, path):
        library_name = "{}ProcessPlugin".format(name.capitalize())
        library_path = os.path.join(path, "lib{}.so".format(library_name))

        return library_path, library_name
