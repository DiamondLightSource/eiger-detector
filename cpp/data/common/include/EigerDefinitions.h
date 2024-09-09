/*
 * EigerDefinitions.h
 *
 *  Created on: 6 Apr 2017
 *      Author: Matt Taylor
 */

#ifndef INCLUDE_EIGERDEFINITIONS_H_
#define INCLUDE_EIGERDEFINITIONS_H_

#include <stdint.h>
#include "version.h"

namespace Eiger {

  // Eiger constants
  const std::string STREAM_PORT_NUMBER = "9999";

  const std::string GLOBAL_HEADER_TYPE = "dheader-1.0";
  const std::string IMAGE_HEADER_TYPE = "dimage-1.0";
  const std::string END_HEADER_TYPE = "dseries_end-1.0";

  const std::string HEADER_DETAIL_ALL = "all";
  const std::string HEADER_DETAIL_BASIC = "basic";
  const std::string HEADER_DETAIL_NONE = "none";

  const std::string HEADER_TYPE_KEY = "htype";
  const std::string HEADER_DETAIL_KEY = "header_detail";
  const std::string SERIES_KEY = "series";
  const std::string FRAME_KEY = "frame";
  const std::string SHAPE_KEY = "shape";
  const std::string DATA_TYPE_KEY = "type";
  const std::string HASH_KEY = "hash";
  const std::string ENCODING_KEY = "encoding";
  const std::string SIZE_KEY = "size";
  const std::string START_TIME_KEY = "start_time";
  const std::string STOP_TIME_KEY = "stop_time";
  const std::string REAL_TIME_KEY = "real_time";

  const std::string ACQUISITION_ID_KEY = "acqID";

  const std::string END_STREAM_MESSAGE = "{\"htype\": \"dseries_end-1.0\", \"series\": 1}";

  // EigerFan related constants
  const int MORE_MESSAGES = 1;
  const int RECEIVE_HWM = 100000;  // High water marks for the main receiver thread
  const int SEND_HWM = 100000;
  const int WORKER_SEND_HWM = 10000;  // A lower high water mark for the worker threads
  const int LINGER_TIMEOUT = 100;  // Socket linger timeout in milliseconds

  const std::string CONTROL_CMD_KEY = "msg_val";
  const std::string CONTROL_ID_KEY = "id";
  const std::string CONTROL_PARAM_KEY = "params";

  const std::string CONTROL_STATUS = "status";
  const std::string CONTROL_REQ_CONFIG = "request_configuration";
  const std::string CONTROL_REQ_VERSION = "request_version";
  const std::string CONTROL_CONFIGURE = "configure";
  const std::string CONTROL_KILL = "kill";
  const std::string CONTROL_CLOSE = "close";
  const std::string CONTROL_OFFSET = "offset";
  const std::string CONTROL_ACQ_ID = "acqid";
  const std::string CONTROL_FWD_STREAM = "forward_stream";
  const std::string CONTROL_DEV_SHM_CACHE = "dev_shm_cache";
  const std::string CONTROL_BLOCK_SIZE = "block_size";

  const std::string CONTROL_RESPONSE_OK = "{\"msg_type\":\"ack\",\"msg_val\":\"configure\", \"params\": {}}";
  const std::string CONTROL_RESPONSE_UNABLE = "{\"msg_type\":\"nack\",\"msg_val\":\"configure\", \"params\": {\"error:\":\"Unable to process control command\"}}";
  const std::string CONTROL_RESPONSE_NOPARAM = "{\"msg_type\":\"nack\",\"msg_val\":\"configure\", \"params\": {\"error:\":\"No parameter\"}}";
  const std::string CONTROL_RESPONSE_NOCFGPARAM = "{\"msg_type\":\"nack\",\"msg_val\":\"configure\", \"params\": {\"error:\":\"No recognised configure parameter\"}}";

  enum EigerFanState { WAITING_CONSUMERS,WAITING_STREAM,DSTR_HEADER,DSTR_IMAGE,DSTR_END,KILL_REQUESTED};

  static const std::string STATE_WAITING_CONSUMERS = "WAITING_CONSUMERS";
  static const std::string STATE_WAITING_STREAM = "WAITING_STREAM";
  static const std::string STATE_DSTR_HEADER = "DSTR_HEADER";
  static const std::string STATE_DSTR_IMAGE = "DSTR_IMAGE";
  static const std::string STATE_DSTR_END = "DSTR_END";
  static const std::string STATE_KILL_REQUESTED = "KILL_REQUESTED";

  static const std::string DEV_SHM_PATH = "/dev/shm/eiger";

  enum EigerMessageType { GLOBAL_HEADER_NONE, GLOBAL_HEADER_CONFIG, GLOBAL_HEADER_FLATFIELD, GLOBAL_HEADER_MASK, GLOBAL_HEADER_COUNTRATE, GLOBAL_HEADER_APPENDIX, IMAGE_DATA, IMAGE_APPENDIX, END_OF_STREAM};

  typedef struct
  {
    EigerMessageType messageType;
    int frame_number;
    uint32_t series;

    uint32_t shapeSizeX;
    uint32_t shapeSizeY;
    uint32_t shapeSizeZ;

    uint64_t startTime;
    uint64_t stopTime;
    uint64_t realTime;

    uint32_t data_size;

    uint64_t size_in_header; // Tag in the dimage_d header containing size in bytes
    char hash[33];      // MD5 hash of the message part, written in a 32 byte string
    char encoding[11];  // String of the form "[bs<BIT>][[-]lz4][<|>]"
    char dataType[8];	// "uint16" or "uint32" or "float32"
    char acquisitionID[256];	// acquisitionID
  } FrameHeader;

  static const size_t frame_size_500K    =  2117680 + sizeof(FrameHeader); // 529,420 pixels at 32 bit pixel depth
  static const size_t frame_size_1M      = 4387800 + sizeof(FrameHeader); // 1,096,950 pixels at 32 bit pixel depth
  static const size_t frame_size_4M      = 17942760 + sizeof(FrameHeader); // 4,485,690 pixels at 32 bit pixel depth
  static const size_t frame_size_9M      = 40553184 + sizeof(FrameHeader); // 10,138,296 pixels at 32 bit pixel depth
  static const size_t frame_size_16M     = 72558600 + sizeof(FrameHeader); // 18,139,650 pixels at 32 bit pixel depth

  enum EigerMessageParentType { PARENT_MESSAGE_TYPE_GLOBAL, PARENT_MESSAGE_TYPE_IMAGE_DATA, PARENT_MESSAGE_TYPE_END};

  static const int image_data_imaged_part = 2; // Image dimensions are on the 2nd image part
  static const int image_data_blob_part = 3; // Image data is on the 3rd image part
  static const int image_data_time_part = 4; // Image times are on the 4th image part
  static const int image_data_appendix_part = 5; // Appendix is on the 5th image part

  static const int global_detector_none_part = 1; // Global header first message part
  static const int global_detector_config_part = 2; // Global header 2nd part contains config data
  static const int global_flatfield_header_part = 3; // Global header 3rd part contains flatfield header
  static const int global_flatfield_data_part = 4; // Global header 4th part contains flatfield data
  static const int global_mask_header_part = 5; // Global header 5th part contains pixel mask header
  static const int global_mask_data_part = 6; // Global header 6th part contains pixel mask data
  static const int global_countrate_header_part = 7; // Global header 7th part contains countrate header
  static const int global_countrate_data_part = 8; // Global header 8th part contains countrate data
  static const int global_appendix_part = 9; // Appendix is on the 9th global header part

}

#endif /* INCLUDE_EIGERDEFINITIONS_H_ */
