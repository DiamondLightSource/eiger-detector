from eiger_detector.data.eiger_meta_writer import EigerMetaWriter
from odin_data.meta_writer.meta_writer import MetaWriterConfig


def test_eiger_meta_writer():
    writer = EigerMetaWriter(
        "writer", "/tmp", [], MetaWriterConfig(sensor_shape=(4362, 4148))
    )
    assert writer._name == "writer"
