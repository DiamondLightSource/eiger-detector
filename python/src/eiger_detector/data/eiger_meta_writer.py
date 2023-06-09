"""Implementation of Eiger Meta Writer

This module is a subclass of the odin_data MetaWriter and handles Eiger specific meta messages, writing them to disk.

Matt Taylor, Diamond Light Source
"""
import cbor2
from odin_data.meta_writer.hdf5dataset import (
    Float32HDF5Dataset,
    Float64HDF5Dataset,
    UInt32HDF5Dataset,
    UInt64HDF5Dataset,
    StringHDF5Dataset,
)
from odin_data.meta_writer.meta_writer import MetaWriter
from odin_data.util import construct_version_dict

from eiger_detector._version import get_versions

from .stream2 import stream2_tag_decoder

# Stream2 message parameters
TYPE = "type"
START_TYPE = "start"
END_TYPE = "end"

# Stream2 image message parameters
START_TIME = "start_time"
STOP_TIME = "stop_time"
REAL_TIME = "real_time"
IMAGE_ID = "image_id"
SERIES_ID = "series_id"
SIZE = "size"
COMPRESSION = "compression"
DATATYPE = "type"

# Stream2 start message parameters

## Simple datasets
SERIES_UNIQUE_ID = "series_unique_id"
ARM_DATE = "arm_date"
BEAM_CENTER_Y = "beam_center_y"
BEAM_CENTER_X = "beam_center_x"
COUNT_TIME = "count_time"
COUNTRATE_CORRECTION_ENABLED = "countrate_correction_enabled"
DETECTOR_DESCRIPTION = "detector_description"
DETECTOR_TRANSLATION = "detector_translation"
DETECTOR_SERIAL_NUMBER = "detector_serial_number"
FLATFIELD_ENABLED = "flatfield_enabled"
FRAME_TIME = "frame_time"
IMAGE_SIZE_X = "image_size_x"
IMAGE_SIZE_Y = "image_size_y"
IMAGES_PER_TRIGGER = "images_per_trigger"
INCIDENT_ENERGY = "incident_energy"
INCIDENT_WAVELENGTH = "incident_wavelength"
NUMBER_OF_IMAGES = "number_of_images"
NUMBER_OF_TRIGGERS = "number_of_triggers"
PIXEL_MASK_ENABLED = "pixel_mask_enabled"
PIXEL_SIZE_X = "pixel_size_x"
PIXEL_SIZE_Y = "pixel_size_y"
ROI_MODE = "roi_mode"
SATURATION_VALUE = "saturation_value"
SENSOR_MATERIAL = "sensor_material"
SENSOR_THICKNESS = "sensor_thickness"
VIRTUAL_PIXEL_INTERPOLATION_ENABLED = "virtual_pixel_interpolation_enabled"
SIMPLE_DATASETS = [
    SERIES_UNIQUE_ID,
    ARM_DATE,
    BEAM_CENTER_Y,
    BEAM_CENTER_X,
    COUNT_TIME,
    COUNTRATE_CORRECTION_ENABLED,
    DETECTOR_DESCRIPTION,
    DETECTOR_TRANSLATION,
    DETECTOR_SERIAL_NUMBER,
    FLATFIELD_ENABLED,
    FRAME_TIME,
    IMAGE_SIZE_X,
    IMAGE_SIZE_Y,
    INCIDENT_ENERGY,
    INCIDENT_WAVELENGTH,
    NUMBER_OF_IMAGES,
    PIXEL_MASK_ENABLED,
    PIXEL_SIZE_X,
    PIXEL_SIZE_Y,
    SATURATION_VALUE,
    SENSOR_MATERIAL,
    SENSOR_THICKNESS,
    VIRTUAL_PIXEL_INTERPOLATION_ENABLED,
]

## Datasets that need some unpacking
# TODO: It could be nice to handle these dynamically from `channels`, but we need to
# create them on file open for SWMR currently
THRESHOLD_1 = "threshold_1"
THRESHOLD_2 = "threshold_2"
COUNTRATE_CORRECTION = "countrate_correction_lookup_table"
FLATFIELD = "flatfield"
FLATFIELD_1 = f"{FLATFIELD}/{THRESHOLD_1}"
FLATFIELD_2 = f"{FLATFIELD}/{THRESHOLD_2}"
PIXEL_MASK = "pixel_mask"
PIXEL_MASK_1 = f"{PIXEL_MASK}/{THRESHOLD_1}"
PIXEL_MASK_2 = f"{PIXEL_MASK}/{THRESHOLD_2}"
THRESHOLD_ENERGY = "threshold_energy"
THRESHOLD_ENERGY_1 = f"{THRESHOLD_ENERGY}/{THRESHOLD_1}"
THRESHOLD_ENERGY_2 = f"{THRESHOLD_ENERGY}/{THRESHOLD_2}"

### Goniometer datasets
CHI = "chi"
KAPPA = "kappa"
OMEGA = "omega"
PHI = "phi"
TWO_THETA = "two_theta"
GONIOMETER_DATASETS = [CHI, KAPPA, OMEGA, PHI, TWO_THETA]


def dectris(suffix):
    return "_dectris/{}".format(suffix)


class EigerMetaWriter(MetaWriter):
    """Implementation of MetaWriter that also handles Eiger meta messages"""

    # Define parameters received on per frame meta message for parent class
    DETECTOR_WRITE_FRAME_PARAMETERS = [
        IMAGE_ID,
        START_TIME,
        STOP_TIME,
        REAL_TIME,
        SIZE,
        COMPRESSION,
        DATATYPE,
    ]

    def __init__(self, name, directory, endpoints, config):
        # This must be defined for _define_detector_datasets in base class __init__
        self._sensor_shape = config.sensor_shape

        super(EigerMetaWriter, self).__init__(name, directory, endpoints, config)
        self._detector_finished = False  # Require base class to check we have finished

        self._series_number = None

    def _define_detector_datasets(self):
        return [
            # Datasets with one value received per frame
            UInt64HDF5Dataset(IMAGE_ID, block_size=1000),
            UInt64HDF5Dataset(START_TIME, block_size=1000),
            UInt64HDF5Dataset(STOP_TIME, block_size=1000),
            UInt64HDF5Dataset(REAL_TIME, block_size=1000),
            # SIZE will be N arrays of 1-3 values depending on threshold configuration
            UInt64HDF5Dataset(SIZE, shape=(0, 3), maxshape=(None, 3), block_size=1000),
            StringHDF5Dataset(COMPRESSION, block_size=1000),
            StringHDF5Dataset(DATATYPE, block_size=1000),
            # Datasets received on arm
            UInt64HDF5Dataset(SERIES_ID, cache=False),
            StringHDF5Dataset(dectris(SERIES_UNIQUE_ID), cache=False),
            StringHDF5Dataset(dectris(ARM_DATE), cache=False),
            Float64HDF5Dataset(dectris(BEAM_CENTER_X), cache=False),
            Float64HDF5Dataset(dectris(BEAM_CENTER_Y), cache=False),
            Float64HDF5Dataset(dectris(COUNT_TIME), cache=False),
            UInt64HDF5Dataset(dectris(COUNTRATE_CORRECTION_ENABLED), cache=False),
            StringHDF5Dataset(dectris(DETECTOR_DESCRIPTION), cache=False),
            Float64HDF5Dataset(dectris(DETECTOR_TRANSLATION), cache=False),
            StringHDF5Dataset(dectris(DETECTOR_SERIAL_NUMBER), cache=False),
            UInt64HDF5Dataset(dectris(FLATFIELD_ENABLED), cache=False),
            Float64HDF5Dataset(dectris(FRAME_TIME), cache=False),
            UInt64HDF5Dataset(dectris(IMAGE_SIZE_X), cache=False),
            UInt64HDF5Dataset(dectris(IMAGE_SIZE_Y), cache=False),
            UInt64HDF5Dataset(dectris(IMAGES_PER_TRIGGER), cache=False),
            Float64HDF5Dataset(dectris(INCIDENT_ENERGY), cache=False),
            Float64HDF5Dataset(dectris(INCIDENT_WAVELENGTH), cache=False),
            UInt64HDF5Dataset(dectris(NUMBER_OF_IMAGES), cache=False),
            UInt64HDF5Dataset(dectris(NUMBER_OF_TRIGGERS), cache=False),
            UInt64HDF5Dataset(dectris(PIXEL_MASK_ENABLED), cache=False),
            Float64HDF5Dataset(dectris(PIXEL_SIZE_X), cache=False),
            Float64HDF5Dataset(dectris(PIXEL_SIZE_Y), cache=False),
            StringHDF5Dataset(dectris(ROI_MODE), cache=False),
            UInt64HDF5Dataset(dectris(SATURATION_VALUE), cache=False),
            StringHDF5Dataset(dectris(SENSOR_MATERIAL), cache=False),
            Float64HDF5Dataset(dectris(SENSOR_THICKNESS), cache=False),
            UInt64HDF5Dataset(dectris(VIRTUAL_PIXEL_INTERPOLATION_ENABLED), cache=False),
            UInt64HDF5Dataset(dectris(COUNTRATE_CORRECTION), cache=False),
            Float32HDF5Dataset(dectris(FLATFIELD_1), shape=self._sensor_shape, rank=2, cache=False),
            Float32HDF5Dataset(dectris(FLATFIELD_2), shape=self._sensor_shape, rank=2, cache=False),
            UInt32HDF5Dataset(dectris(PIXEL_MASK_1), shape=self._sensor_shape, rank=2, cache=False),
            UInt32HDF5Dataset(dectris(PIXEL_MASK_2), shape=self._sensor_shape, rank=2, cache=False),
            Float64HDF5Dataset(dectris(THRESHOLD_ENERGY_1), cache=False),
            Float64HDF5Dataset(dectris(THRESHOLD_ENERGY_2), cache=False),
        ]

    @property
    def _data_datasets(self):
        return ["data1", "data2", "data3"]

    @property
    def detector_message_handlers(self):
        return {
            "eiger-start": self.handle_cbor_message,
            "eiger-image": self.handle_image_message,
            "eiger-end": self.handle_cbor_message,
        }

    def handle_cbor_message(self, header: dict, data: bytes):
        data = cbor2.loads(data, tag_hook=stream2_tag_decoder)
        if data[TYPE] == START_TYPE:
            self.handle_start_message(header, data)
        elif data[TYPE] == END_TYPE:
            self.handle_end_message(header, data)
        else:
            raise ValueError(f"Unknown message type {data[TYPE]}")

    def handle_start_message(self, header: dict, data: dict):
        self._logger.debug("%s | Handling start message", self._name)

        if not self.file_open:
            self._logger.warning(
                "%s | File not open for eiger start message. Creating now.", self._name
            )
            self._create_file(self._generate_full_file_path(), 0)

        self._series_number = header[SERIES_ID]
        self._write_dataset(SERIES_ID, header[SERIES_ID])

        # Datasets that map straight through to the `data` layout
        self._write_datasets(
            # Add prefix to dataset names and keys of data dictionary
            [dectris(parameter) for parameter in SIMPLE_DATASETS],
            dict((dectris(key), value) for key, value in data.items()),
        )

        # Array datasets
        self._write_dataset(dectris(COUNTRATE_CORRECTION), data[COUNTRATE_CORRECTION])
        self._write_dataset(dectris(FLATFIELD_1), data[FLATFIELD][THRESHOLD_1])
        self._write_dataset(dectris(PIXEL_MASK_1), data[PIXEL_MASK][THRESHOLD_1])
        self._write_dataset(
            dectris(THRESHOLD_ENERGY_1), data[THRESHOLD_ENERGY][THRESHOLD_1]
        )
        # There may or may not be meta data for a second threshold
        if THRESHOLD_2 in data[FLATFIELD]:
            self._write_dataset(dectris(FLATFIELD_2), data[FLATFIELD][THRESHOLD_2])
        if THRESHOLD_2 in data[PIXEL_MASK]:
            self._write_dataset(dectris(PIXEL_MASK_2), data[PIXEL_MASK][THRESHOLD_2])
        if THRESHOLD_2 in data[THRESHOLD_ENERGY]:
            self._write_dataset(
                dectris(THRESHOLD_ENERGY_2), data[THRESHOLD_ENERGY][THRESHOLD_2]
            )

        # Goniometer axis parameters
        for axis_name in GONIOMETER_DATASETS:
            try:
                axis = data["goniometer"][axis_name]
            except KeyError:
                self._logger.warning(
                    "%s | %s not configured on detector", self._name, axis_name
                )
                continue

            self._add_dataset(
                dectris(f"goniometer/{axis_name}_start"), axis["start"]
            )
            self._add_dataset(
                dectris(f"goniometer/{axis_name}_increment"), axis["increment"]
            )

    def handle_image_message(self, header: dict, data: dict):
        """Handle per-image data message from EigerProcessPlugin"""
        self._logger.debug("%s | Handling image data message", self._name)

        if not self._series_valid(header[SERIES_ID]):
            return

        if data[IMAGE_ID] in self._frame_offset_map:
            self._logger.warning(
                "%s | Base class has already written data for frame %d",
                self._name,
                data[IMAGE_ID],
            )
            offset = self._frame_offset_map.pop(data[IMAGE_ID])
            self._add_values(self.DETECTOR_WRITE_FRAME_PARAMETERS, data, offset)
        else:
            # Store this to be written in write_detector_frame_data
            # This will be called when handle_write_frame is called in the
            # base class with this frame number
            self._frame_data_map[data[IMAGE_ID]] = data

    def handle_end_message(self, header: dict, _data: dict):
        """Handle end message - register to stop when writers finished"""
        self._logger.debug("%s | Handling end message", self._name)

        if self._series_valid(header[SERIES_ID]):
            self.stop_when_writers_finished()

    def _series_valid(self, series_number):
        if self._series_number == series_number:
            return True
        else:
            self._logger.error(
                "%s | Received unexpected message from series %s - %s",
                self._name,
                series_number,
                "expected series {}".format(self._series_number)
                if self._series_number is not None
                else "have not received start message with series",
            )
            return False

    @staticmethod
    def get_version():
        return ("eiger-detector", construct_version_dict(get_versions()["version"]))
