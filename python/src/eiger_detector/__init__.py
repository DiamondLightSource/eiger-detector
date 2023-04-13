from eiger_detector._version import get_versions
from eiger_detector.control.eiger_adapter import EigerAdapter
from eiger_detector.control.eiger_fan_adapter import EigerFanAdapter
from eiger_detector.data.eiger_meta_writer import EigerMetaWriter

__version__ = get_versions()["version"]
del get_versions

__all__ = ["EigerAdapter", "EigerFanAdapter", "EigerMetaWriter", "__version__"]
