"""Implementation of Eiger Meta Writer

This module is a subclass of the odin_data MetaWriter and handles Eiger specific meta messages, writing them to disk.

Matt Taylor, Diamond Light Source
"""
import re
from json import loads

import numpy as np

from odin_data.meta_writer.meta_writer import MetaWriter, MESSAGE_TYPE_ID, FRAME
from odin_data.meta_writer.hdf5dataset import Int64HDF5Dataset, StringHDF5Dataset
from odin_data.util import construct_version_dict
import _version as versioneer

# Data message parameters
START_TIME = "start_time"
STOP_TIME = "stop_time"
REAL_TIME = "real_time"
SERIES = "series"
SIZE = "size"
HASH = "hash"
ENCODING = "encoding"
DATATYPE = "type"


class EigerMetaWriter(MetaWriter):
    """Implementation of MetaWriter that also handles Eiger meta messages"""

    # Define Eiger-specific datasets
    DETECTOR_DATASETS = [
        Int64HDF5Dataset(START_TIME),
        Int64HDF5Dataset(STOP_TIME),
        Int64HDF5Dataset(REAL_TIME),
        Int64HDF5Dataset(SERIES),
        Int64HDF5Dataset(SIZE),
        StringHDF5Dataset(HASH, length=32),
        StringHDF5Dataset(ENCODING, length=10),
        StringHDF5Dataset(DATATYPE, length=6),
    ]
    # Define parameters received on per frame meta message
    DETECTOR_WRITE_FRAME_PARAMETERS = [
        START_TIME,
        STOP_TIME,
        REAL_TIME,
        SERIES,
        SIZE,
        HASH,
        ENCODING,
        DATATYPE,
    ]

    def __init__(self, *args, **kwargs):
        super(EigerMetaWriter, self).__init__(*args, **kwargs)
        self._detector_finished = False  # Require base class to check we have finished

    @property
    def detector_message_handlers(self):
        return {
            "eiger-globalnone": self.handle_global_header_none,
            "eiger-globalconfig": self.handle_global_header_config,
            "eiger-globalflatfield": self.handle_flatfield_header,
            "eiger-globalmask": self.handle_mask_header,
            "eiger-globalcountrate": self.handle_countrate_header,
            "eiger-headerappendix": self.handle_header_appendix,
            "eiger-imagedata": self.handle_image_data,
            "eiger-imageappendix": self.handle_image_appendix,
            "eiger-end": self.handle_end,
        }

    def handle_global_header_none(self, _header, data):
        """Handle global header message (header_detail = none)"""
        self._logger.debug("%s | Handling global header none message", self._name)

        self._add_dataset(SERIES, np.array(data[SERIES]))

    def handle_global_header_config(self, _header, data):
        """Handle global header config part message containing config data"""
        self._logger.debug("%s | Handling global header config message", self._name)

        self._add_dataset("config", data=str(data))
        for parameter, value in data.items():
            self._add_dataset("_dectris/{}".format(parameter), value)

    def handle_flatfield_header(self, _header, flatfield_blob):
        """Handle global header flatfield part message containing flatfield array"""
        self._logger.debug("%s | Handling flatfield header message", self._name)

        flatfield_array = np.frombuffer(flatfield_blob, dtype=np.float32)
        self._add_dataset("flatfield", flatfield_array)

    def handle_mask_header(self, _header, mask_blob):
        """Handle global header mask part message containing mask array"""
        self._logger.debug("%s | Handling mask header message", self._name)

        mask_array = np.frombuffer(mask_blob, dtype=np.uint32)
        self._add_dataset("mask", mask_array)

    def handle_countrate_header(self, _header, countrate_blob):
        """Handle global header mask part message containing mask array"""
        self._logger.debug("%s | Handling countrate header message", self._name)

        countrate_table = np.frombuffer(countrate_blob, dtype=np.float32)
        self._add_dataset("countrate", countrate_table)

    def handle_header_appendix(self, _header, data):
        """Handle global header appendix part message"""
        self._logger.debug("%s | Handling header appendix message", self._name)

        appendix = np.str(data)
        self._add_dataset("global_appendix", appendix)

    def handle_image_data(self, _header, data):
        """Handle image data message"""
        self._logger.debug("%s | Handling image data message", self._name)

        # Store this to be written in write_detector_frame_data
        # This will be called when handle_write_frame is called in the
        # base class with this frame number
        self._frame_data_map[data[FRAME]] = data

    def handle_image_appendix(self, _header, _data):
        """Handle image appendix message"""
        self._logger.debug("%s | Handling image appendix message", self._name)
        # Do nothing as can't write variable length dataset in swmr

    def handle_end(self, _header, _data):
        """Handle end message - register to stop when writers finished"""
        self._logger.debug("%s | Handling end message", self._name)
        self.stop_when_writers_finished()

    @staticmethod
    def get_version():
        return construct_version_dict(versioneer.get_versions()["version"])
