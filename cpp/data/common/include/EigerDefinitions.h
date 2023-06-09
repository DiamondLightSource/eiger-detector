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

namespace Eiger
{

  // Eiger constants
  const std::string STREAM_PORT_NUMBER = "31001";

  const std::string SERIES_KEY = "series_number";

  const std::string ACQUISITION_ID_KEY = "acqID";

  const std::string END_STREAM_MESSAGE = "{\"htype\": \"dseries_end-1.0\", \"series\": 1}";

  // EigerFan related constants
  const int MORE_MESSAGES = 1;
  const int RECEIVE_HWM = 10000;
  const int SEND_HWM = 10000;

  const std::string CONTROL_CMD_KEY = "msg_val";
  const std::string CONTROL_ID_KEY = "id";
  const std::string CONTROL_PARAM_KEY = "params";

  const std::string CONTROL_STATUS = "status";
  const std::string CONTROL_REQ_CONFIG = "request_configuration";
  const std::string CONTROL_REQ_VERSION = "request_version";
  const std::string CONTROL_REQ_COMMANDS = "request_commands";
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

  enum EigerFanState
  {
    WAITING_CONSUMERS,
    WAITING_STREAM,
    DSTR_HEADER,
    DSTR_IMAGE,
    DSTR_END,
    KILL_REQUESTED
  };

  static const std::string STATE_WAITING_CONSUMERS = "WAITING_CONSUMERS";
  static const std::string STATE_WAITING_STREAM = "WAITING_STREAM";
  static const std::string STATE_DSTR_HEADER = "DSTR_HEADER";
  static const std::string STATE_DSTR_IMAGE = "DSTR_IMAGE";
  static const std::string STATE_DSTR_END = "DSTR_END";
  static const std::string STATE_KILL_REQUESTED = "KILL_REQUESTED";

  static const std::string DEV_SHM_PATH = "/dev/shm/eiger";

  enum EigerMessageType
  {
    GLOBAL_HEADER_NONE,
    GLOBAL_HEADER_CONFIG,
    GLOBAL_HEADER_FLATFIELD,
    GLOBAL_HEADER_MASK,
    GLOBAL_HEADER_COUNTRATE,
    GLOBAL_HEADER_APPENDIX,
    IMAGE_DATA,
    IMAGE_APPENDIX,
    END_OF_STREAM
  };

  enum EigerMessageParentType
  {
    PARENT_MESSAGE_TYPE_GLOBAL,
    PARENT_MESSAGE_TYPE_IMAGE_DATA,
    PARENT_MESSAGE_TYPE_END
  };
}

#endif /* INCLUDE_EIGERDEFINITIONS_H_ */
