"""Implementation of Eiger Meta Writer

This module is a subclass of the odin_data MetaWriter and handles Eiger specific meta messages, writing them to disk.

Matt Taylor, Diamond Light Source
"""
import numpy as np
import time
import re

from odin_data.meta_writer.meta_writer import MetaWriter
import _version as versioneer

MAJOR_VER_REGEX = r"^([0-9]+)[\\.-].*|$"
MINOR_VER_REGEX = r"^[0-9]+[\\.-]([0-9]+).*|$"
PATCH_VER_REGEX = r"^[0-9]+[\\.-][0-9]+[\\.-]([0-9]+).|$"

class EigerMetaWriter(MetaWriter):
    """Eiger Meta Writer class.

    Eiger Detector Meta Writer writes Eiger meta data to disk
    """

    def __init__(self, logger, directory, acquisitionID):
        """Initalise the EigerMetaWriter object.

        :param logger: Logger to use
        :param directory: Directory to create the meta file in
        :param acquisitionID: Acquisition ID of this acquisition
        """
        super(EigerMetaWriter, self).__init__(logger, directory, acquisitionID)

        self.add_dataset_definition("start_time", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)
        self.add_dataset_definition("stop_time", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)
        self.add_dataset_definition("real_time", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)
        self.add_dataset_definition("frame", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)
        self.add_dataset_definition("size", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)
        self.add_dataset_definition("hash", (0,), maxshape=(None,), dtype='S32', fillvalue=None)
        self.add_dataset_definition("encoding", (0,), maxshape=(None,), dtype='S10', fillvalue=None)
        self.add_dataset_definition("datatype", (0,), maxshape=(None,), dtype='S6', fillvalue=None)
        self.add_dataset_definition("frame_series", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)
        self.add_dataset_definition("frame_written", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)
        self.add_dataset_definition("offset_written", (0,), maxshape=(None,), dtype='int64', fillvalue=-1)

        self._num_frame_offsets_written = 0
        self._current_frame_count = 0
        self._need_to_write_data = False
        self._arrays_created = False
        self._close_after_write = False
        self._frame_offset_dict = {}
        self._frame_data_dict = {}

        self._series_created = False
        self._config_created = False
        self._flatfield_created = False
        self._pixel_mask_created = False
        self._countrate_created = False
        self._global_appendix_created = False

        self.start_new_acquisition()
        
    @staticmethod    
    def get_version():
      
        version = versioneer.get_versions()["version"]
        major_version = re.findall(MAJOR_VER_REGEX, version)[0]
        minor_version = re.findall(MINOR_VER_REGEX, version)[0]
        patch_version = re.findall(PATCH_VER_REGEX, version)[0]
        short_version = major_version + "." + minor_version + "." + patch_version
        
        version_dict = {}
        version_dict["full"] = version
        version_dict["major"] = major_version
        version_dict["minor"] = minor_version
        version_dict["patch"] = patch_version
        version_dict["short"] = short_version
        return version_dict

    def start_new_acquisition(self):
        """Performs actions needed when the acquisition is started."""

        self._frame_offset_dict.clear()
        self._frame_data_dict.clear()

        self._series_created = False
        self._config_created = False
        self._flatfield_created = False
        self._pixel_mask_created = False
        self._countrate_created = False
        self._global_appendix_created = False

        return

    def handle_global_header_none(self, message):
        """Handle global header message with details flag set to None.

        :param message: The message received
        """
        self._logger.debug('Handling global header none for acqID ' + self._acquisition_id)
        self._logger.debug(message)

        if self._series_created:
            self._logger.debug('series already created')
            return

        if not self.file_created:
            self.create_file()

        npa = np.array(message['series'])
        self.create_dataset_with_data("series", data=npa)
        self._series_created = True

        return

    def handle_global_header_config(self, header, config):
        """Handle global header config part message containing config data.

        :param header: The header received
        :param config: The config data
        """
        self._logger.debug('Handling global header cfg for acqID ' + self._acquisition_id)
        self._logger.debug(header)
        self._logger.debug(config)

        if not self.file_created:
            self.create_file()

        if self._config_created:
            self._logger.debug('config already created')
        else:
            nps = np.str(config)
            self.create_dataset_with_data("config", data=nps)
            self._config_created = True

        if self._series_created:
            self._logger.debug('series already created')
        else:
            npa = np.array(header['series'])
            self.create_dataset_with_data("series", data=npa)
            self._series_created = True

        return

    def handle_flatfield_header(self, header, flatfield):
        """Handle global header flatfield part message containing flatfield data.

        :param header: The header received
        :param flatfield: The flatfield data
        """
        self._logger.debug('Handling flatfield header for acqID ' + self._acquisition_id)
        self._logger.debug(header)

        if self._flatfield_created:
            self._logger.debug('flatfield already created')
            return

        if not self.file_created:
            self.create_file()

        self._flatfield_created = True
        npa = np.frombuffer(flatfield, dtype=np.float32)
        shape = header['shape']
        self.create_dataset_with_data("flatfield", data=npa, shape=(shape[1], shape[0]))
        return

    def handle_mask_header(self, header, mask):
        """Handle global header pixel mask part message containing pixel mask data.

        :param header: The header received
        :param mask: The pixel mask data
        """
        self._logger.debug('Handling mask header for acqID ' + self._acquisition_id)
        self._logger.debug(header)

        if self._pixel_mask_created:
            self._logger.debug('pixel mask already created')
            return

        if not self.file_created:
            self.create_file()

        self._pixel_mask_created = True

        npa = np.frombuffer(mask, dtype=np.uint32)
        shape = header['shape']
        self.create_dataset_with_data("mask", data=npa, shape=(shape[1], shape[0]))
        return

    def handle_countrate_header(self, header, countrate):
        """Handle global header count rate part message containing count rate data.

        :param header: The header received
        :param countrate: The count rate data
        """
        self._logger.debug('Handling countrate header for acqID ' + self._acquisition_id)
        self._logger.debug(header)

        if self._countrate_created:
            self._logger.debug('countrate already created')
            return

        if not self.file_created:
            self.create_file()

        self._countrate_created = True

        npa = np.frombuffer(countrate, dtype=np.float32)
        shape = header['shape']
        self.create_dataset_with_data("countrate", data=npa, shape=(shape[1], shape[0]))
        return

    def handle_global_header_appendix(self, appendix):
        """Handle global header appendix part message.

        :param appendix: The appendix data
        """
        self._logger.debug('Handling global header appendix for acqID ' + self._acquisition_id)

        if self._global_appendix_created:
            self._logger.debug('global appendix already created')
            return

        if not self.file_created:
            self.create_file()

        self._global_appendix_created = True

        nps = np.str(appendix)
        self.create_dataset_with_data("globalAppendix", data=nps)
        return

    def handle_data(self, header):
        """Handle meta data message.

        :param header: The header
        """
        frame_id = header['frame']

        # Check if we know the offset to write to yet, if so write the frame, if not store the data until we do know.
        if self._frame_offset_dict.has_key(frame_id) == True:
            self.write_frame_data(self._frame_offset_dict[frame_id], header)
            del self._frame_offset_dict[frame_id]
            if self._close_after_write:
                self.close_file()
        else:
            self._frame_data_dict[frame_id] = header

        return

    def handle_image_appendix(self, header, appendix):
        """Handle meta data message appendix message part.

        :param header: The header
        :param appendix: The appendix data
        """
        self._logger.debug('Handling image appendix for acqID ' + self._acquisition_id)
        self._logger.debug(header)
        self._logger.debug(appendix)
        # Do nothing as can't write variable length dataset in swmr
        return

    def handle_end(self, message):
        """Handle end of series message.

        :param message: The message
        """
        self._logger.debug('Handling end for acqID ' + self._acquisition_id)
        self._logger.debug(message)
        # Do nothing with end message
        return

    def handle_frame_writer_start_acquisition(self, userHeader):
        """Handle frame writer plugin start acquisition message.

        :param userHeader: The header
        """
        self._logger.debug('Handling frame writer start acquisition for acqID ' + self._acquisition_id)
        self._logger.debug(userHeader)

        self.number_processes_running = self.number_processes_running + 1

        if not self.file_created:
            self.create_file()

        if self._num_frames_to_write == -1:
            self._num_frames_to_write = userHeader['totalFrames']
            self.create_arrays()

        return

    def handle_frame_writer_create_file(self, userHeader, fileName):
        """Handle frame writer plugin create file message.

        :param userHeader: The header
        :param fileName: The file name
        """
        self._logger.debug('Handling frame writer create file for acqID ' + self._acquisition_id)
        self._logger.debug(userHeader)
        self._logger.debug(fileName)

        return

    def handle_frame_writer_write_frame(self, message):
        """Handle frame writer plugin write frame message.

        :param message: The message
        """
        frame_number = message['frame']
        offset_value = message['offset']

        if not self._arrays_created:
            self._logger.error('Arrays not created, cannot handle frame writer data')
            return

        offset_to_write_to = offset_value

        if self._num_frame_offsets_written + 1 > self._num_frames_to_write:
            self._data_set_arrays["frame_written"] = np.resize(self._data_set_arrays["frame_written"],
                                                               (self._num_frame_offsets_written + 1,))
            self._data_set_arrays["offset_written"] = np.resize(self._data_set_arrays["offset_written"],
                                                                (self._num_frame_offsets_written + 1,))

        self._data_set_arrays["frame_written"][self._num_frame_offsets_written] = frame_number
        self._data_set_arrays["offset_written"][self._num_frame_offsets_written] = offset_to_write_to

        self._num_frame_offsets_written = self._num_frame_offsets_written + 1

        # Check if we have the data and/or appendix for this frame yet. If so, write it in the offset given
        if self._frame_data_dict.has_key(frame_number):
            self.write_frame_data(offset_to_write_to, self._frame_data_dict[frame_number])
            del self._frame_data_dict[frame_number]
        else:
            self._frame_offset_dict[frame_number] = offset_to_write_to

        return

    def create_arrays(self):
        """Create the data set arrays for all of the Eiger meta datasets."""
        self._data_set_arrays["start_time"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))
        self._data_set_arrays["stop_time"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))
        self._data_set_arrays["real_time"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))
        self._data_set_arrays["frame"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))
        self._data_set_arrays["size"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))
        self._data_set_arrays["hash"] = np.empty(self._num_frames_to_write, dtype='S32')
        self._data_set_arrays["encoding"] = np.empty(self._num_frames_to_write, dtype='S10')
        self._data_set_arrays["datatype"] = np.empty(self._num_frames_to_write, dtype='S6')
        self._data_set_arrays["frame_series"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))

        self._data_set_arrays["frame_written"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))
        self._data_set_arrays["offset_written"] = np.negative(np.ones((self._num_frames_to_write,), dtype=np.int64))

        self._hdf5_datasets["start_time"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["stop_time"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["real_time"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["frame"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["size"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["hash"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["encoding"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["datatype"].resize(self._num_frames_to_write, axis=0)
        self._hdf5_datasets["frame_series"].resize(self._num_frames_to_write, axis=0)

        self._hdf5_file.swmr_mode = True

        self._arrays_created = True

    def handle_frame_writer_close_file(self):
        """Handle frame writer plugin close file message."""
        self._logger.debug('Handling frame writer close file for acqID ' + self._acquisition_id)
        # Do nothing
        return

    def close_file(self):
        """Close the file."""
        if len(self._frame_offset_dict) > 0:
            # Writers have finished but we haven't got all associated meta. Wait till it comes before closing
            self._logger.info('Unable to close file as Frame Offset Dict Length = ' + str(len(self._frame_offset_dict)))
            self._close_after_write = True
            return

        self.write_datasets()

        if self._hdf5_file is not None:
            self._logger.info('Closing file ' + self.full_file_name)
            self._hdf5_file.close()
            self._logger.info('Meta frames written: ' + str(self._current_frame_count) + ' of ' + str(self._num_frames_to_write))
            self._hdf5_file = None

        self.finished = True

    def handle_frame_writer_stop_acquisition(self, userheader):
        """Handle frame writer plugin stop acquisition message.

        :param userheader: The user header
        """
        self._logger.debug('Handling frame writer stop acquisition for acqID ' + self._acquisition_id)
        self._logger.debug(userheader)

        if self.number_processes_running > 0:
            self.number_processes_running = self.number_processes_running - 1

        if self.number_processes_running == 0:
            self._logger.info('Last processor ended for acqID ' + str(self._acquisition_id))
            if self._current_frame_count >= self._num_frames_to_write:
                self.close_file()
            else:
                self._logger.info(
                    'Not closing file as not all frames written (' + str(self.write_count) + ' of ' + str(
                        self._num_frames_to_write) + ')')
        else:
            self._logger.info('Processor ended, but not the last for acqID ' + str(self._acquisition_id))

        return

    def write_frame_data(self, offset, header):
        """Write the frame data to the arrays and flush if necessary.

        :param offset: The offset to write to in the arrays
        :param header: The data header
        """
        if not self._arrays_created:
            self._logger.error('Arrays not created, cannot write frame data')
            return

        if offset + 1 > self._current_frame_count:
            self._current_frame_count = offset + 1

        self._data_set_arrays["start_time"][offset] = header['start_time']
        self._data_set_arrays["stop_time"][offset] = header['stop_time']
        self._data_set_arrays["real_time"][offset] = header['real_time']
        self._data_set_arrays["frame"][offset] = header['frame']
        self._data_set_arrays["size"][offset] = header['size']
        self._data_set_arrays["hash"][offset] = header['hash']
        self._data_set_arrays["encoding"][offset] = header['encoding']
        self._data_set_arrays["datatype"][offset] = header['type']
        self._data_set_arrays["frame_series"][offset] = header['series']

        self.write_count = self.write_count + 1
        self._need_to_write_data = True

        flush = False
        if self.flush_timeout is not None:
            if (time.time() - self._last_flushed) >= self.flush_timeout:
                flush = True
        elif (self.write_count % self.flush_frequency) == 0:
            flush = True

        if flush:
            self.write_datasets()

        # Reset timeout count to 0
        self.write_timeout_count = 0

        return

    def write_datasets(self):
        """Write the datasets to the hdf5 file."""
        if not self._arrays_created:
            self._logger.warn('Arrays not created, cannot write datasets from frame data')
            return

        if self._need_to_write_data:
            self._logger.info('Writing data to datasets at write count ' + str(self.write_count) + ' for acqID ' + str(self._acquisition_id))
            self._hdf5_datasets["start_time"][0:self._num_frames_to_write] = self._data_set_arrays["start_time"]
            self._hdf5_datasets["stop_time"][0:self._num_frames_to_write] = self._data_set_arrays["stop_time"]
            self._hdf5_datasets["real_time"][0:self._num_frames_to_write] = self._data_set_arrays["real_time"]
            self._hdf5_datasets["frame"][0:self._num_frames_to_write] = self._data_set_arrays["frame"]
            self._hdf5_datasets["size"][0:self._num_frames_to_write] = self._data_set_arrays["size"]
            self._hdf5_datasets["hash"][0:self._num_frames_to_write] = self._data_set_arrays["hash"]
            self._hdf5_datasets["encoding"][0:self._num_frames_to_write] = self._data_set_arrays["encoding"]
            self._hdf5_datasets["datatype"][0:self._num_frames_to_write] = self._data_set_arrays["datatype"]
            self._hdf5_datasets["frame_series"][0:self._num_frames_to_write] = self._data_set_arrays["frame_series"]

            self._hdf5_datasets["frame_written"].resize(self._num_frame_offsets_written, axis=0)
            self._hdf5_datasets["frame_written"][0:self._num_frame_offsets_written] = self._data_set_arrays["frame_written"][0:self._num_frame_offsets_written]

            self._hdf5_datasets["offset_written"].resize(self._num_frame_offsets_written, axis=0)
            self._hdf5_datasets["offset_written"][0:self._num_frame_offsets_written] = self._data_set_arrays["offset_written"][0:self._num_frame_offsets_written]

            self._hdf5_datasets["start_time"].flush()
            self._hdf5_datasets["stop_time"].flush()
            self._hdf5_datasets["real_time"].flush()
            self._hdf5_datasets["frame"].flush()
            self._hdf5_datasets["size"].flush()
            self._hdf5_datasets["hash"].flush()
            self._hdf5_datasets["encoding"].flush()
            self._hdf5_datasets["datatype"].flush()
            self._hdf5_datasets["frame_series"].flush()
            self._hdf5_datasets["frame_written"].flush()
            self._hdf5_datasets["offset_written"].flush()

            self._last_flushed = time.time()

            self._need_to_write_data = False

    def stop(self):
        """Stop this acquisition."""
        self._frame_offset_dict.clear()
        self.close_file()

    def process_message(self, message, userheader, receiver):
        """Process a meta message.

        :param message: The message
        :param userheader: The user header
        :param receiver: The ZeroMQ socket the data was received on
        """
        self._logger.debug('Eiger Meta Writer Handling message')

        if message['parameter'] == "eiger-globalnone":
            receiver.recv_json()
            self.handle_global_header_none(message)
        elif message['parameter'] == "eiger-globalconfig":
            config = receiver.recv_json()
            self.handle_global_header_config(userheader, config)
        elif message['parameter'] == "eiger-globalflatfield":
            flatfield = receiver.recv()
            self.handle_flatfield_header(userheader, flatfield)
        elif message['parameter'] == "eiger-globalmask":
            mask = receiver.recv()
            self.handle_mask_header(userheader, mask)
        elif message['parameter'] == "eiger-globalcountrate":
            countrate = receiver.recv()
            self.handle_countrate_header(userheader, countrate)
        elif message['parameter'] == "eiger-headerappendix":
            appendix = receiver.recv()
            self.handle_global_header_appendix(appendix)
        elif message['parameter'] == "eiger-imagedata":
            imageMetaData = receiver.recv_json()
            self.handle_data(imageMetaData)
        elif message['parameter'] == "eiger-imageappendix":
            appendix = receiver.recv()
            self.handle_image_appendix(userheader, appendix)
        elif message['parameter'] == "eiger-end":
            receiver.recv()
            self.handle_end(message)
        elif message['parameter'] == "createfile":
            fileName = receiver.recv()
            self.handle_frame_writer_create_file(userheader, fileName)
        elif message['parameter'] == "closefile":
            receiver.recv()
            self.handle_frame_writer_close_file()
        elif message['parameter'] == "startacquisition":
            receiver.recv()
            self.handle_frame_writer_start_acquisition(userheader)
        elif message['parameter'] == "stopacquisition":
            receiver.recv()
            self.handle_frame_writer_stop_acquisition(userheader)
        elif message['parameter'] == "writeframe":
            value = receiver.recv_json()
            self.handle_frame_writer_write_frame(value)
        else:
            self._logger.error('unknown parameter: ' + str(message))
            value = receiver.recv()
            self._logger.error('value: ' + str(value))

        return