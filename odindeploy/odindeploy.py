import os
import json
import uuid
from string import Template
from argparse import ArgumentParser, RawTextHelpFormatter


_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
TEMPLATE_DIR = os.path.join(_SCRIPT_DIR, "templates")
OUTPUT_DIR = os.path.join(_SCRIPT_DIR, "build")


class OdinPaths(object):

    ODIN_DATA_ROOT = ""
    EIGER_ROOT = ""
    HDF5_FILTERS = ""

    @classmethod
    def configure_paths(cls, config):
        cls.ODIN_DATA_ROOT = config["ODIN_DATA_ROOT"]
        cls.EIGER_ROOT = config["EIGER_ROOT"]
        cls.HDF5_FILTERS = config["HDF5_FILTERS"]


SERVERS = 1
PROCESSES = 4


description = """A script to generate an Odin deployment

Provide JSON file defining a single dictionary with the following parameters:

    * ODIN_DATA_ROOT - Path to odin-data directory
    * EIGER_ROOT - Path to eiger-detector directory
    * HDF5_FILTERS - Path to directory containing HDF5 dynamic compression libraries
    * DETECTOR_IP - IP Address of Detector

The following parameters are optional and have defaults:

    * DETECTOR_CTRL_PORT - Port for detector control endpoint [80]

"""


def main():

    parser = ArgumentParser(
        description=description, formatter_class=RawTextHelpFormatter
    )
    parser.add_argument("config_file")
    args = parser.parse_args()

    with open(args.config_file) as json_file:
        config = json.load(json_file)

    OdinPaths.configure_paths(config)

    if not os.path.isdir(OUTPUT_DIR):
        os.mkdir(OUTPUT_DIR)

    if "DETECTOR_CTRL_PORT" not in config:
        config["DETECTOR_CTRL_PORT"] = 80
    detector_ctrl_endpoint = "{}:{}".format(
        config["DETECTOR_IP"], config["DETECTOR_CTRL_PORT"]
    )

    fan = EigerFan(ip="127.0.0.1", detector_ip=config["DETECTOR_IP"],
                   processes=PROCESSES, sockets=4, sensor="4M")
    data_server = EigerOdinDataServer(ip="127.0.0.1", source=fan,
                                      shared_mem_size=160000000, io_threads=2)
    meta = EigerMetaListener(ip="127.0.0.1", odin_data_server_1=data_server)
    control_server = EigerOdinControlServer(ip="127.0.0.1", eiger_fan=fan,
                                            meta_listener=meta,
                                            odin_data_server_1=data_server,
                                            detector_api="1.6.0",
                                            detector_endpoint=detector_ctrl_endpoint)
    log_config = OdinLogConfig()

    data_server.configure_processes(server_rank=0, total_servers=1)
    od_idx = 0
    for odin_data in data_server.processes:
        odin_data.create_config_files(index=od_idx)
        od_idx += SERVERS

    data_server.create_od_startup_scripts()
    control_server.create_config_file()
    create_start_all_script(PROCESSES)


def create_start_all_script(processes):
    scripts = []
    commands = []
    for process_number in range(1, processes + 1):
        scripts.append(
            "FP{number}=\"${{SCRIPT_DIR}}/stFrameProcessor{number}.sh\"".format(
                number=process_number)
        )
        scripts.append(
            "FR{number}=\"${{SCRIPT_DIR}}/stFrameReceiver{number}.sh\"".format(
                number=process_number)
        )
        commands.append(
            "gnome-terminal --tab --title=\"FP{number}\" -- bash -c \"${{FP{number}}}\"".format(number=process_number)
        )
        commands.append(
            "gnome-terminal --tab --title=\"FR{number}\" -- bash -c \"${{FR{number}}}\"".format(number=process_number)
        )
    macros = dict(EIGER_ROOT=OdinPaths.EIGER_ROOT, ODIN_DATA_ROOT=OdinPaths.ODIN_DATA_ROOT,
                  ODIN_DATA_SCRIPTS="\n".join(scripts),
                  ODIN_DATA_COMMANDS="\n".join(commands))
    expand_template_file("odin.sh", macros, "stOdin.sh", executable=True)


class _OdinData(object):

    """Store configuration for an OdinData process"""
    INDEX = 1  # Unique index for each OdinData instance
    RANK = None
    FP_ENDPOINT = ""
    FR_ENDPOINT = ""

    def __init__(self, server, ready, release, meta, plugins):
        self.ip = server.ip
        self.ready = ready
        self.release = release
        self.meta = meta
        self.plugins = plugins

        self.index = _OdinData.INDEX
        _OdinData.INDEX += 1

    def create_config_file(self, prefix, template, extra_macros=None):
        macros = dict(
            IP=self.ip, OD_ROOT=OdinPaths.ODIN_DATA_ROOT,
            RD_PORT=self.ready, RL_PORT=self.release, META_PORT=self.meta
        )
        if extra_macros is not None:
            macros.update(extra_macros)
        if self.plugins is not None:
            load_entries = []
            connect_entries = []
            config_entries = []
            for plugin in self.plugins:
                load_entries.append(plugin.create_config_load_entry())
                connect_entries.append(create_config_entry(plugin.create_config_connect_entry()))
                config_entries += plugin.create_extra_config_entries(self.RANK)
            for mode in self.plugins.modes:
                valid_entries = False
                mode_config_dict = {'store': {'index': mode, 'value': [{'plugin': {'disconnect': 'all'}}]}}
                for plugin in self.plugins:
                    entry = plugin.create_config_connect_entry(mode)
                    if entry is not None:
                        valid_entries = True
                        mode_config_dict['store']['value'].append(entry)
                if valid_entries:
                    connect_entries.append(create_config_entry(mode_config_dict))

            custom_plugin_config_macros = dict(
                LOAD_ENTRIES=",\n  ".join(load_entries),
                CONNECT_ENTRIES=",\n  ".join(connect_entries),
                CONFIG_ENTRIES=",\n  ".join(config_entries)
            )
            macros.update(custom_plugin_config_macros)

        expand_template_file(template, macros, "{}{}.json".format(prefix, self.RANK + 1))

    def create_config_files(self, index):
        raise NotImplementedError("Method must be implemented by child classes")


class FrameProcessorPlugin(object):

    NAME = None
    CLASS_NAME = None
    LIBRARY_NAME = None

    TEMPLATE = None
    TEMPLATE_INSTANTIATED = False

    @property
    def ROOT_PATH(self):
        return OdinPaths.ODIN_DATA_ROOT

    def __init__(self, source=None):
        self.connections = {}

        if source is not None:
            self.source = source.NAME
        else:
            self.source = "frame_receiver"

    def add_mode(self, mode, source=None):
        if source is not None:
            self.connections[mode] = source.NAME
        else:
            self.connections[mode] = "frame_receiver"

    def create_config_load_entry(self):
        library_name = self.LIBRARY_NAME if self.LIBRARY_NAME is not None else self.CLASS_NAME
        entry = {
            "plugin": {
                "load": {
                    "index": self.NAME,
                    "name": self.CLASS_NAME,
                    "library": "{}/prefix/lib/lib{}.so".format(self.ROOT_PATH, library_name)
                }
            }
        }
        return create_config_entry(entry)

    def create_config_connect_entry(self, mode=None):
        connection = None
        if mode is None:
            connection = self.source
        elif mode in self.connections:
            connection = self.connections[mode]

        entry = None
        if connection is not None:
            entry = {
                "plugin": {
                    "connect": {
                        "index": self.NAME,
                        "connection": connection,
                    }
                }
            }
        return entry

    def create_extra_config_entries(self, rank):
        return []


class PluginConfig(object):

    def __init__(self,
                 plugin_1=None, plugin_2=None, plugin_3=None, plugin_4=None,
                 plugin_5=None, plugin_6=None, plugin_7=None, plugin_8=None):
        self.plugins = [plugin for plugin in
                        [plugin_1, plugin_2, plugin_3, plugin_4,
                         plugin_5, plugin_6, plugin_7, plugin_8]
                        if plugin is not None]
        self.modes = []

    def detector_setup(self, od_args):
        # No op, should be overridden by specific detector
        pass

    def __iter__(self):
        for plugin in self.plugins:
            yield plugin


class _OdinDataServer(object):

    """Store configuration for an OdinDataServer"""
    PORT_BASE = 5000
    PROCESS_COUNT = 0

    def __init__(self, ip, processes, shared_mem_size, plugin_config=None,
                 io_threads=1, total_numa_nodes=0):
        self.ip = ip
        self.shared_mem_size = shared_mem_size
        self.plugins = plugin_config
        self.io_threads = io_threads
        self.total_numa_nodes = total_numa_nodes

        self.processes = []
        for _ in range(processes):
            self.processes.append(
                self.create_odin_data_process(
                    self, self.PORT_BASE + 1, self.PORT_BASE + 2,
                          self.PORT_BASE + 8, plugin_config)
            )
            self.PORT_BASE += 10

        self.instantiated = False  # Make sure instances are only used once

    def create_odin_data_process(self, server, ready, release, meta,
                                 plugin_config):
        raise NotImplementedError("Method must be implemented by child classes")

    def configure_processes(self, server_rank, total_servers):
        rank = server_rank
        for idx, process in enumerate(self.processes):
            process.RANK = rank
            rank += total_servers

    def create_od_startup_scripts(self):
        for idx, process in enumerate(self.processes):
            fp_port_number = 5004 + (10 * idx)
            fr_port_number = 5000 + (10 * idx)
            ready_port_number = 5001 + (10 * idx)
            release_port_number = 5002 + (10 * idx)

            # If total_numa_nodes was set, we enable the NUMA call macro instantitation
            if self.total_numa_nodes > 0:
                numa_node = idx % int(self.total_numa_nodes)
                numa_call = "numactl --membind={node} --cpunodebind={node} ".format(node=numa_node)
            else:
                numa_call = ""

            # Store server designation on OdinData object
            process.FP_ENDPOINT = "{}:{}".format(self.ip, fp_port_number)
            process.FR_ENDPOINT = "{}:{}".format(self.ip, fr_port_number)

            output_file = "stFrameReceiver{}.sh".format(process.RANK + 1)
            macros = dict(
                NUMBER=process.RANK + 1,
                OD_ROOT=OdinPaths.ODIN_DATA_ROOT,
                BUFFER_IDX=idx + 1, SHARED_MEMORY=self.shared_mem_size,
                CTRL_PORT=fr_port_number, IO_THREADS=self.io_threads,
                READY_PORT=ready_port_number, RELEASE_PORT=release_port_number,
                LOG_CONFIG=os.path.join(OUTPUT_DIR, "log4cxx.xml"),
                NUMA=numa_call)
            expand_template_file("fr.sh", macros, output_file,
                                 executable=True)

            output_file = "stFrameProcessor{}.sh".format(process.RANK + 1)
            macros = dict(
                NUMBER=process.RANK + 1,
                OD_ROOT=OdinPaths.ODIN_DATA_ROOT,
                HDF5_FILTERS=OdinPaths.HDF5_FILTERS,
                CTRL_PORT=fp_port_number,
                READY_PORT=ready_port_number, RELEASE_PORT=release_port_number,
                LOG_CONFIG=os.path.join(OUTPUT_DIR, "log4cxx.xml"),
                NUMA=numa_call)
            expand_template_file("fp.sh", macros, output_file,
                                 executable=True)


class OdinLogConfig(object):
    """Create logging configuration file"""

    def __init__(self):

        self.create_config_file()

    def create_config_file(self):
        expand_template_file("log4cxx.xml", {}, "log4cxx.xml")


# ~~~~~~~~~~~ #
# OdinControl #
# ~~~~~~~~~~~ #

class _OdinControlServer(object):
    """Store configuration for an OdinControlServer"""

    ODIN_SERVER = None
    ADAPTERS = ["fp", "fr"]

    # Device attributes
    AutoInstantiate = True

    def __init__(self, ip, port=8888,
                 odin_data_server_1=None, odin_data_server_2=None,
                 odin_data_server_3=None, odin_data_server_4=None):
        self.ip = ip
        self.port = port

        self.odin_data_servers = [
            server for server in [
                odin_data_server_1, odin_data_server_2, odin_data_server_3,
                odin_data_server_4
            ] if server is not None
        ]

        if not self.odin_data_servers:
            raise ValueError("Received no control endpoints for Odin Server")

        self.odin_data_processes = []
        for server in self.odin_data_servers:
            if server is not None:
                self.odin_data_processes += server.processes

        self.create_startup_script()

    def get_extra_startup_macro(self):
        return ""

    def create_startup_script(self):
        macros = dict(ODIN_SERVER=self.ODIN_SERVER, CONFIG="odin_server.cfg",
                      EXTRA_PARAMS=self.get_extra_startup_macro())
        expand_template_file("odin_server.sh", macros, "stOdinServer.sh",
                             executable=True)

    def create_config_file(self):
        macros = dict(PORT=self.port,
                      ADAPTERS=", ".join(self.ADAPTERS),
                      ADAPTER_CONFIG="".join(
                          self.create_odin_server_config_entries()))
        expand_template_file("odin_server.ini", macros, "odin_server.cfg")

    def create_odin_server_config_entries(self):
        raise NotImplementedError(
            "Method must be implemented by child classes")

    def _create_odin_data_config_entry(self):
        fp_endpoints = []
        fr_endpoints = []
        for process in sorted(self.odin_data_processes, key=lambda x: x.RANK):
            fp_endpoints.append(process.FP_ENDPOINT)
            fr_endpoints.append(process.FR_ENDPOINT)

        return """
[adapter.fp]
module = odin_data.frame_processor_adapter.FrameProcessorAdapter
endpoints = {}
update_interval = 0.2

[adapter.fr]
module = odin_data.frame_receiver_adapter.FrameReceiverAdapter
endpoints = {}
update_interval = 0.2
""".format(", ".join(fp_endpoints), ", ".join(fr_endpoints))


class FileWriterPlugin(FrameProcessorPlugin):

    NAME = "hdf"
    CLASS_NAME = "FileWriterPlugin"
    LIBRARY_NAME = "Hdf5Plugin"
    DATASET_NAME = "data"

    def __init__(self, source=None, indexes=False):
        super(FileWriterPlugin, self).__init__(source)

        self.indexes = indexes

    def create_extra_config_entries(self, rank):
        entries = []
        dataset_entry = {
            self.NAME: {
                "dataset": self.DATASET_NAME,
            }
        }
        entries.append(create_config_entry(dataset_entry))
        if self.indexes:
            indexes_entry = {
                self.NAME: {
                    "dataset": {
                        self.DATASET_NAME: {
                            "indexes": True
                        }
                    }
                }
            }
            entries.append(create_config_entry(indexes_entry))

        return entries


class DatasetCreationPlugin(FrameProcessorPlugin):

    DATASET_NAME = None
    DATASET_TYPE = "uint64"

    def create_extra_config_entries(self, rank):
        entries = super(DatasetCreationPlugin, self).create_extra_config_entries(rank)
        dataset_entry = {
            FileWriterPlugin.NAME: {
                "dataset": {
                    self.DATASET_NAME: {
                        "chunks": OneLineEntry([1000]),
                        "datatype": self.DATASET_TYPE
                    }
                }
            }
        }
        entries.append(create_config_entry(dataset_entry))

        return entries


class EigerProcessPlugin(DatasetCreationPlugin):

    NAME = "eiger"
    CLASS_NAME = "EigerProcessPlugin"
    DATASET_NAME = "compressed_size"
    DATASET_TYPE = "uint32"

    @property
    def ROOT_PATH(self):
        return OdinPaths.EIGER_ROOT

    def __init__(self, size_dataset):
        super(EigerProcessPlugin, self).__init__(None)

        self.size_dataset = size_dataset

    def create_extra_config_entries(self, rank):
        entries = []
        if self.size_dataset:
            entries = super(EigerProcessPlugin, self).create_extra_config_entries(rank)

        return entries


class EigerFan(object):

    """Create startup file for an EigerFan process"""

    def __init__(self, ip, detector_ip, processes, sockets, sensor, threads=2,
                 block_size=1000, numa_node=-1):
        self.ip = ip
        self.detector_ip = detector_ip
        self.processes = processes
        self.sockets = sockets
        self.sensor = sensor
        self.threads = threads
        self.block_size = block_size
        self.numa_node = numa_node

        self.create_startup_file()

    def create_startup_file(self):
        if self.numa_node >= 0:
            numa_call = "numactl --membind={node} --cpunodebind={node} ".format(
                node=self.numa_node)
        else:
            numa_call = ""
        macros = dict(EIGER_DETECTOR_PATH=OdinPaths.EIGER_ROOT, IP=self.detector_ip,
                      PROCESSES=self.processes, SOCKETS=self.sockets,
                      BLOCK_SIZE=self.block_size,
                      THREADS=self.threads,
                      LOG_CONFIG=os.path.join(OdinPaths.EIGER_ROOT, "log4cxx.xml"),
                      NUMA=numa_call)

        expand_template_file("eiger_fan.sh", macros, "stEigerFan.sh",
                             executable=True)


class EigerMetaListener(object):

    """Create startup file for an EigerMetaListener process"""

    def __init__(self, ip,
                 odin_data_server_1=None, odin_data_server_2=None,
                 odin_data_server_3=None, odin_data_server_4=None,
                 numa_node=-1):
        self.ip = ip
        self.numa_node = numa_node

        self.ip_list = []
        self.sensor = None
        for server in [odin_data_server_1, odin_data_server_2,
                       odin_data_server_3, odin_data_server_4]:
            if server is not None:
                base_port = 5000
                for odin_data in server.processes:
                    port = base_port + 8
                    self.ip_list.append(
                        "tcp://{}:{}".format(odin_data.ip, port))
                    base_port += 10
                    self.set_sensor(odin_data.sensor)

        self.create_startup_file()

    def set_sensor(self, sensor):
        if self.sensor is None:
            self.sensor = sensor
        else:
            if self.sensor != sensor:
                raise ValueError(
                    "Inconsistent sensor sizes given on OdinData processes")

    def create_startup_file(self):
        if self.numa_node >= 0:
            numa_call = "numactl --membind={node} --cpunodebind={node} ".format(
                node=self.numa_node)
        else:
            numa_call = ""
        macros = dict(EIGER_DETECTOR_PATH=OdinPaths.EIGER_ROOT,
                      IP_LIST=",".join(self.ip_list),
                      OD_ROOT=OdinPaths.ODIN_DATA_ROOT,
                      SENSOR=self.sensor,
                      NUMA=numa_call)

        expand_template_file("eiger_meta.sh", macros,
                             "stEigerMetaListener.sh",
                             executable=True)


class _EigerOdinData(_OdinData):

    def __init__(self, server, ready, release, meta, plugin_config, source_ip,
                 sensor):
        super(_EigerOdinData, self).__init__(server, ready, release, meta,
                                             plugin_config)
        self.source = source_ip
        self.sensor = sensor

    def create_config_files(self, index):
        macros = dict(DETECTOR_ROOT=OdinPaths.EIGER_ROOT,
                      IP=self.source,
                      RX_PORT_SUFFIX=self.RANK,
                      SENSOR=self.sensor)

        super(_EigerOdinData, self).create_config_file(
            "fp", "fp.json", extra_macros=macros)
        super(_EigerOdinData, self).create_config_file(
            "fr", "fr.json", extra_macros=macros)


class EigerPluginConfig(PluginConfig):

    def __init__(self, mode="Simple"):
        if mode == "Simple":
            eiger = EigerProcessPlugin(size_dataset=False)
            hdf = FileWriterPlugin(source=eiger, indexes=True)
            plugins = [eiger, hdf]
        else:
            raise ValueError("Invalid mode for EigerPluginConfig")

        super(EigerPluginConfig, self).__init__(*plugins)


class EigerOdinDataServer(_OdinDataServer):

    """Store configuration for an EigerOdinDataServer"""

    PLUGIN_CONFIG = None

    def __init__(self, ip, source, shared_mem_size=16000000000,
                 plugin_config=None, io_threads=1, total_numa_nodes=0):
        self.source = source.ip
        self.sensor = source.sensor
        if plugin_config is None:
            if EigerOdinDataServer.PLUGIN_CONFIG is None:
                # Create the standard Eiger plugin config
                EigerOdinDataServer.PLUGIN_CONFIG = EigerPluginConfig()
        else:
            EigerOdinDataServer.PLUGIN_CONFIG = plugin_config

        super(EigerOdinDataServer, self).__init__(
            ip, source.processes, shared_mem_size,
            EigerOdinDataServer.PLUGIN_CONFIG, io_threads, total_numa_nodes
        )

    def create_odin_data_process(self, server, ready, release, meta,
                                 plugin_config):
        return _EigerOdinData(server, ready, release, meta, plugin_config,
                              self.source, self.sensor)


class EigerOdinControlServer(_OdinControlServer):

    """Store configuration for an EigerOdinControlServer"""

    @property
    def ODIN_SERVER(self):
        return os.path.join(OdinPaths.EIGER_ROOT, "prefix/bin/eiger_odin")

    def __init__(self, ip, eiger_fan, meta_listener, detector_endpoint, detector_api,
                 port=8888,
                 odin_data_server_1=None, odin_data_server_2=None,
                 odin_data_server_3=None, odin_data_server_4=None):
        self.ADAPTERS.extend(["eiger_fan", "meta_listener"])

        self.detector_api = detector_api
        self.detector_endpoint = detector_endpoint
        self.eiger_fan = eiger_fan
        self.meta_listener = meta_listener

        super(EigerOdinControlServer, self).__init__(
            ip, port, odin_data_server_1, odin_data_server_2,
            odin_data_server_3, odin_data_server_4
        )

    def create_odin_server_config_entries(self):
        return [
            self._create_control_config_entry(),
            self._create_odin_data_config_entry(),
            self._create_eiger_fan_config_entry(),
            self._create_meta_listener_config_entry()
        ]

    def _create_control_config_entry(self):
        return """
[adapter.eiger]
module = eiger.eiger_adapter.EigerAdapter
endpoint = {}
api = {}
""".format(self.detector_endpoint, self.detector_api)

    def _create_eiger_fan_config_entry(self):
        return """
[adapter.eiger_fan]
module = eiger.eiger_fan_adapter.EigerFanAdapter
endpoints = {}:5559
update_interval = 0.5
""".format(self.eiger_fan.ip)

    def _create_meta_listener_config_entry(self):
        return """
[adapter.meta_listener]
module = odin_data.meta_listener_adapter.MetaListenerAdapter
endpoints = {}:5659
update_interval = 0.5
""".format(self.meta_listener.ip)


def expand_template_file(template, macros, output_file_name, executable=False):

    output_file_path = os.path.join(OUTPUT_DIR, output_file_name)

    with open(os.path.join(TEMPLATE_DIR, template)) as template_file:
        template_config = Template(template_file.read())

    output = template_config.substitute(macros)

    with open(output_file_path, mode="w") as output_file:
        output_file.write(output)

    if executable:
        os.chmod(output_file_path, 0o0755)


class OneLineEntry(object):

    """A wrapper to stop JSON entries being split across multiple lines.

    Wrap this around lists, dictionaries, etc to stop json.dumps from
    splitting them over multiple lines. Must pass OneLineEncoder to
    json.dumps(cls=).

    """

    def __init__(self, value):
        self.value = value


class OneLineEncoder(json.JSONEncoder):

    def __init__(self, *args, **kwargs):
        super(OneLineEncoder, self).__init__(*args, **kwargs)
        self.kwargs = dict(kwargs)
        del self.kwargs["indent"]
        self._replacement_map = {}

    def default(self, o):
        if isinstance(o, OneLineEntry):
            key = uuid.uuid4().hex
            self._replacement_map[key] = json.dumps(o.value, **self.kwargs)
            return "@@%s@@" % (key,)
        else:
            return super(OneLineEncoder, self).default(o)

    def encode(self, o):
        result = super(OneLineEncoder, self).encode(o)
        for key, value in self._replacement_map.items():
            result = result.replace("\"@@%s@@\"" % (key,), value)
        return result


def create_config_entry(dictionary):
    entry = json.dumps(dictionary, indent=2, cls=OneLineEncoder)
    return entry.replace("\n", "\n  ")


if __name__ == "__main__":
    main()
