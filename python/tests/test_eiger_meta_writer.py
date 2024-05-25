import json
from pathlib import Path

import h5py as h5
from eiger_detector.data.eiger_meta_writer import EigerMetaWriter
from odin_data.meta_writer.meta_writer import MetaWriterConfig

HERE = Path(__file__).parent


def test_file_write(tmp_path):

    writer = EigerMetaWriter(
        "test", tmp_path.as_posix(), [], MetaWriterConfig(sensor_shape=(4362, 4148))
    )

    writer._processes_running = [True]
    writer._endpoints = ["tcp://127.0.0.1:10008"]

    stream_files = sorted((HERE / "input").iterdir())

    # Every other file is header and then data
    for header_path, data_path in zip(stream_files[0::2], stream_files[1::2]):
        with header_path.open("r") as f:
            header = json.load(f)
            header["header"]["_endpoint"] = "tcp://127.0.0.1:10008"

        if data_path.suffix == ".json":
            with data_path.open("r") as f:
                data = json.load(f)
        else:
            with data_path.open("rb") as f:
                data = f.read()

        writer.process_message(header, data)

    with h5.File(tmp_path / "test_meta.h5") as f:
        assert f["series"][0] == 40181
