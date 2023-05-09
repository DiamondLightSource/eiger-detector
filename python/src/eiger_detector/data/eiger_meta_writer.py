"""Implementation of Eiger Meta Writer

This module is a subclass of the odin_data MetaWriter and handles Eiger specific meta messages, writing them to disk.

Matt Taylor, Diamond Light Source
"""

import numpy as np
from odin_data.meta_writer.hdf5dataset import (
    Float32HDF5Dataset,
    Float64HDF5Dataset,
    Int32HDF5Dataset,
    Int64HDF5Dataset,
    StringHDF5Dataset,
)
from odin_data.meta_writer.meta_writer import FRAME, MetaWriter
from odin_data.util import construct_version_dict

from eiger_detector._version import get_versions

# Data message parameters
START_TIME = "start_time"
STOP_TIME = "stop_time"
REAL_TIME = "real_time"
SERIES = "series"
SIZE = "size"
HASH = "hash"
ENCODING = "encoding"
DATATYPE = "type"

# Dectris header parameters
AUTO_SUMMATION = "auto_summation"
BEAM_CENTER_Y = "beam_center_y"
BEAM_CENTER_X = "beam_center_x"
BIT_DEPTH_IMAGE = "bit_depth_image"
BIT_DEPTH_READOUT = "bit_depth_readout"
CHI_INCREMENT = "chi_increment"
CHI_START = "chi_start"
COMPRESSION = "compression"
COUNT_TIME = "count_time"
COUNTRATE_CORRECTION_APPLIED = "countrate_correction_applied"
COUNTRATE_CORRECTION_COUNT_CUTOFF = "countrate_correction_count_cutoff"
DATA_COLLECTION_DATE = "data_collection_date"
DESCRIPTION = "description"
DETECTOR_DISTANCE = "detector_distance"
DETECTOR_NUMBER = "detector_number"
DETECTOR_READOUT_TIME = "detector_readout_time"
DETECTOR_TRANSLATION = "detector_translation"
EIGER_FW_VERSION = "eiger_fw_version"
FLATFIELD_CORRECTION_APPLIED = "flatfield_correction_applied"
FRAME_COUNT_TIME = "frame_count_time"
FRAME_PERIOD = "frame_period"
FRAME_TIME = "frame_time"
KAPPA_INCREMENT = "kappa_increment"
KAPPA_START = "kappa_start"
NIMAGES = "nimages"
NTRIGGER = "ntrigger"
NUMBER_OF_EXCLUDED_PIXELS = "number_of_excluded_pixels"
OMEGA_INCREMENT = "omega_increment"
OMEGA_START = "omega_start"
PHI_INCREMENT = "phi_increment"
PHI_START = "phi_start"
PHOTON_ENERGY = "photon_energy"
PIXEL_MASK_APPLIED = "pixel_mask_applied"
SENSOR_MATERIAL = "sensor_material"
SENSOR_THICKNESS = "sensor_thickness"
SOFTWARE_VERSION = "software_version"
THRESHOLD_ENERGY = "threshold_energy"
TRIGGER_MODE = "trigger_mode"
TWO_THETA_INCREMENT = "two_theta_increment"
TWO_THETA_START = "two_theta_start"
VIRTUAL_PIXEL_CORRECTION_APPLIED = "virtual_pixel_correction_applied"
WAVELENGTH = "wavelength"
X_PIXEL_SIZE = "x_pixel_size"
X_PIXELS_IN_DETECTOR = "x_pixels_in_detector"
Y_PIXEL_SIZE = "y_pixel_size"
Y_PIXELS_IN_DETECTOR = "y_pixels_in_detector"

HEADER_CONFIG = [
    AUTO_SUMMATION,
    BEAM_CENTER_X,
    BEAM_CENTER_Y,
    BIT_DEPTH_IMAGE,
    BIT_DEPTH_READOUT,
    CHI_INCREMENT,
    CHI_START,
    COMPRESSION,
    COUNT_TIME,
    COUNTRATE_CORRECTION_APPLIED,
    COUNTRATE_CORRECTION_COUNT_CUTOFF,
    DATA_COLLECTION_DATE,
    DESCRIPTION,
    DETECTOR_DISTANCE,
    DETECTOR_NUMBER,
    DETECTOR_READOUT_TIME,
    DETECTOR_TRANSLATION,
    EIGER_FW_VERSION,
    FLATFIELD_CORRECTION_APPLIED,
    FRAME_COUNT_TIME,
    FRAME_PERIOD,
    FRAME_TIME,
    KAPPA_INCREMENT,
    KAPPA_START,
    NIMAGES,
    NTRIGGER,
    NUMBER_OF_EXCLUDED_PIXELS,
    OMEGA_INCREMENT,
    OMEGA_START,
    PHI_INCREMENT,
    PHI_START,
    PHOTON_ENERGY,
    PIXEL_MASK_APPLIED,
    SENSOR_MATERIAL,
    SENSOR_THICKNESS,
    SOFTWARE_VERSION,
    THRESHOLD_ENERGY,
    TRIGGER_MODE,
    TWO_THETA_INCREMENT,
    TWO_THETA_START,
    VIRTUAL_PIXEL_CORRECTION_APPLIED,
    WAVELENGTH,
    X_PIXEL_SIZE,
    X_PIXELS_IN_DETECTOR,
    Y_PIXEL_SIZE,
    Y_PIXELS_IN_DETECTOR,
]

# Separate header message datasets
COUNTRATE = "countrate"
FLATFIELD = "flatfield"
MASK = "mask"


def dectris(suffix):
    return "_dectris/{}".format(suffix)


class EigerMetaWriter(MetaWriter):
    """Implementation of MetaWriter that also handles Eiger meta messages"""

    # Define parameters received on per frame meta message for parent class
    DETECTOR_WRITE_FRAME_PARAMETERS = [
        START_TIME,
        STOP_TIME,
        REAL_TIME,
        SIZE,
        HASH,
        ENCODING,
        DATATYPE,
    ]

    def __init__(self, name, directory, endpoints, config):
        # This must be defined for _define_detector_datasets in base class __init__
        self._sensor_shape = config.sensor_shape

        super(EigerMetaWriter, self).__init__(name, directory, endpoints, config)
        self._detector_finished = False  # Require base class to check we have finished

        self._series = None

    def _define_detector_datasets(self):
        return [
            # Datasets with one value received per frame
            Int64HDF5Dataset(START_TIME),
            Int64HDF5Dataset(STOP_TIME),
            Int64HDF5Dataset(REAL_TIME),
            Int64HDF5Dataset(SIZE),
            StringHDF5Dataset(HASH, length=32),
            StringHDF5Dataset(ENCODING, length=10),
            StringHDF5Dataset(DATATYPE, length=6),
            # Datasets received on arm
            Int64HDF5Dataset(SERIES, cache=False),
            Float32HDF5Dataset(COUNTRATE, rank=2, cache=False),
            Float32HDF5Dataset(FLATFIELD, shape=self._sensor_shape, rank=2, cache=False),
            Int32HDF5Dataset(MASK, shape=self._sensor_shape, rank=2, cache=False),
            Int64HDF5Dataset(dectris(AUTO_SUMMATION), cache=False),
            Float64HDF5Dataset(dectris(BEAM_CENTER_X), cache=False),
            Float64HDF5Dataset(dectris(BEAM_CENTER_Y), cache=False),
            Int64HDF5Dataset(dectris(BIT_DEPTH_IMAGE), cache=False),
            Int64HDF5Dataset(dectris(BIT_DEPTH_READOUT), cache=False),
            Int64HDF5Dataset(dectris(CHI_INCREMENT), cache=False),
            Int64HDF5Dataset(dectris(CHI_START), cache=False),
            StringHDF5Dataset(dectris(COMPRESSION), length=6, cache=False),
            Float64HDF5Dataset(dectris(COUNT_TIME), cache=False),
            Int64HDF5Dataset(dectris(COUNTRATE_CORRECTION_APPLIED), cache=False),
            Int64HDF5Dataset(dectris(COUNTRATE_CORRECTION_COUNT_CUTOFF), cache=False),
            StringHDF5Dataset(dectris(DATA_COLLECTION_DATE), length=100, cache=False),
            StringHDF5Dataset(dectris(DESCRIPTION), length=100, cache=False),
            Float64HDF5Dataset(dectris(DETECTOR_DISTANCE), cache=False),
            StringHDF5Dataset(dectris(DETECTOR_NUMBER), length=100, cache=False),
            Int64HDF5Dataset(dectris(DETECTOR_READOUT_TIME), cache=False),
            Float64HDF5Dataset(dectris(DETECTOR_TRANSLATION), cache=False),
            StringHDF5Dataset(dectris(EIGER_FW_VERSION), length=100, cache=False),
            Int64HDF5Dataset(dectris(FLATFIELD_CORRECTION_APPLIED), cache=False),
            Int64HDF5Dataset(dectris(FRAME_COUNT_TIME), cache=False),
            Int64HDF5Dataset(dectris(FRAME_PERIOD), cache=False),
            Int64HDF5Dataset(dectris(FRAME_TIME), cache=False),
            Int64HDF5Dataset(dectris(KAPPA_INCREMENT), cache=False),
            Int64HDF5Dataset(dectris(KAPPA_START), cache=False),
            Int64HDF5Dataset(dectris(NIMAGES), cache=False),
            Int64HDF5Dataset(dectris(NTRIGGER), cache=False),
            Int64HDF5Dataset(dectris(NUMBER_OF_EXCLUDED_PIXELS), cache=False),
            Int64HDF5Dataset(dectris(OMEGA_INCREMENT), cache=False),
            Int64HDF5Dataset(dectris(OMEGA_START), cache=False),
            Int64HDF5Dataset(dectris(PHI_INCREMENT), cache=False),
            Int64HDF5Dataset(dectris(PHI_START), cache=False),
            Float64HDF5Dataset(dectris(PHOTON_ENERGY), cache=False),
            Int64HDF5Dataset(dectris(PIXEL_MASK_APPLIED), cache=False),
            StringHDF5Dataset(dectris(SENSOR_MATERIAL), length=100, cache=False),
            Float64HDF5Dataset(dectris(SENSOR_THICKNESS), cache=False),
            StringHDF5Dataset(dectris(SOFTWARE_VERSION), length=100, cache=False),
            Float64HDF5Dataset(dectris(THRESHOLD_ENERGY), cache=False),
            StringHDF5Dataset(dectris(TRIGGER_MODE), length=4, cache=False),
            Int64HDF5Dataset(dectris(TWO_THETA_INCREMENT), cache=False),
            Int64HDF5Dataset(dectris(TWO_THETA_START), cache=False),
            Int64HDF5Dataset(dectris(VIRTUAL_PIXEL_CORRECTION_APPLIED), cache=False),
            Float64HDF5Dataset(dectris(WAVELENGTH), cache=False),
            Float64HDF5Dataset(dectris(X_PIXEL_SIZE), cache=False),
            Int64HDF5Dataset(dectris(X_PIXELS_IN_DETECTOR), cache=False),
            Float64HDF5Dataset(dectris(Y_PIXEL_SIZE), cache=False),
            Int64HDF5Dataset(dectris(Y_PIXELS_IN_DETECTOR), cache=False),
        ]

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

    def handle_global_header_none(self, header, _data):
        """Handle global header message part 1"""
        self._logger.debug("%s | Handling global header none message", self._name)

        if not self.file_open:
            self._logger.warning(
                "%s | File not open for eiger start message. Creating now.", self._name
            )
            self._create_file(self._generate_full_file_path(), 0)

        # Register the series we are expecting in proceding messages
        self._series = header[SERIES]
        self._write_dataset(SERIES, header[SERIES])

    def handle_global_header_config(self, header, data):
        """Handle global header message part (1 and) 2 containing config data"""
        self._logger.debug("%s | Handling global header config message", self._name)

        self.handle_global_header_none(header, data)

        self._add_dataset("config", data=str(data))
        self._write_datasets(
            # Add prefix to HEADER_CONFIG parameters and keys of data dictionary
            [dectris(parameter) for parameter in HEADER_CONFIG],
            dict((dectris(key), value) for key, value in data.items()),
        )

        # This is the last message of the global header with header_detail basic
        self._flush_datasets()

    def handle_flatfield_header(self, header, flatfield_blob):
        """Handle global header parts 3 and 4 containing flatfield array"""
        self._logger.debug("%s | Handling flatfield header message", self._name)

        shape = tuple(reversed(header["shape"]))  # (x, y) -> (y, x)
        flatfield_array = np.frombuffer(flatfield_blob, dtype=np.float32).reshape(shape)
        self._write_dataset(FLATFIELD, flatfield_array)

    def handle_mask_header(self, header, mask_blob):
        """Handle global header parts 5 and 6 containing mask array"""
        self._logger.debug("%s | Handling mask header message", self._name)

        shape = tuple(reversed(header["shape"]))  # (x, y) -> (y, x)
        mask_array = np.frombuffer(mask_blob, dtype=np.uint32).reshape(shape)
        self._write_dataset(MASK, mask_array)

    def handle_countrate_header(self, header, countrate_blob):
        """Handle global header parts 7 and 8 containing mask array"""
        self._logger.debug("%s | Handling countrate header message", self._name)

        shape = tuple(reversed(header["shape"]))  # (x, y) -> (y, x)
        countrate_table = np.frombuffer(countrate_blob, dtype=np.float32).reshape(shape)
        self._write_dataset(COUNTRATE, countrate_table)

        # This is the last message of the global header with header_detail all
        self._flush_datasets()

    def handle_header_appendix(self, _header, data):
        """Handle global header appendix part message"""
        self._logger.debug("%s | Handling header appendix message", self._name)

        appendix = str(data)
        self._add_dataset("global_appendix", appendix)

    def handle_image_data(self, header, data):
        """Handle image data message parts 1, 2 and 4"""
        self._logger.debug("%s | Handling image data message", self._name)

        if self._series_valid(header):
            if data[FRAME] in self._frame_offset_map:
                self._logger.warning(
                    "%s | Base class has already written data for frame %d",
                    self._name,
                    header[FRAME],
                )
                offset = self._frame_offset_map.pop(data[FRAME])
                self._add_values(self.DETECTOR_WRITE_FRAME_PARAMETERS, data, offset)
            else:
                # Store this to be written in write_detector_frame_data
                # This will be called when handle_write_frame is called in the
                # base class with this frame number
                self._frame_data_map[data[FRAME]] = data

    def handle_image_appendix(self, _header, _data):
        """Handle image appendix message"""
        self._logger.debug("%s | Handling image appendix message", self._name)
        # Do nothing as can't write variable length dataset in swmr

    def handle_end(self, header, _data):
        """Handle end message - register to stop when writers finished"""
        self._logger.debug("%s | Handling end message", self._name)

        if self._series_valid(header):
            self.stop_when_writers_finished()

    def _series_valid(self, header):
        if self._series == header[SERIES]:
            return True
        else:
            self._logger.error(
                "%s | Received unexpected message from series %s - %s",
                self._name,
                header[SERIES],
                "expected series {}".format(self._series)
                if self._series is not None
                else "have not received header message with series",
            )
            return False

    @staticmethod
    def get_version():
        return ("eiger-detector", construct_version_dict(get_versions()["version"]))
