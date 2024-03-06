/*
 * EigerFrameDecoder.h
 *
 *  Created on: 23 Feb 2017
 *      Author: Alan Greer
 */

#ifndef SRC_EIGERFRAMEDECODER_H_
#define SRC_EIGERFRAMEDECODER_H_

#include "FrameDecoderZMQ.h"
#include "EigerDefinitions.h"
#include "gettime.h"
#include <stdint.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <time.h>
#include <arpa/inet.h>
#include <boost/format.hpp>
#include "rapidjson/document.h"

namespace FrameReceiver
{

  class EigerFrameDecoder : public FrameDecoderZMQ
  {
  public:
    EigerFrameDecoder();
    virtual ~EigerFrameDecoder();
    void init(LoggerPtr& logger, OdinData::IpcMessage& config_msg);
    const size_t get_frame_buffer_size(void) const;
    const size_t get_frame_header_size(void) const;

    void* get_next_message_buffer(void);
    size_t get_next_payload_size(void) const;
    FrameDecoder::FrameReceiveState process_message(size_t bytes_received);
    void frame_meta_data(int meta);

    void monitor_buffers(void);
    void get_status(const std::string param_prefix, OdinData::IpcMessage& status_msg);
    void request_configuration(const std::string param_prefix, OdinData::IpcMessage& config_reply);

    void* get_packet_header_buffer(void);

    int get_version_major();
    int get_version_minor();
    int get_version_patch();
    std::string get_version_short();
    std::string get_version_long();

  private:

    unsigned int elapsed_ms(struct timespec& start, struct timespec& end);
    void allocate_next_frame_buffer(void);
    void send_buffer(void);

    FrameDecoder::FrameReceiveState process_global_header_message(size_t bytes_received);

    FrameDecoder::FrameReceiveState process_image_message(size_t bytes_received);

    FrameDecoder::FrameReceiveState process_end_message(size_t bytes_received);

    boost::shared_ptr<void> current_raw_buffer_;
    boost::shared_ptr<void> dropped_frame_buffer_;

    void *current_frame_buffer_;

    int current_frame_number_;
    int current_frame_buffer_id_;

    bool dropping_frame_data_;

    std::string detector_model_;
    size_t buffer_size;

    unsigned int frames_allocated_;

    Eiger::EigerMessageType currentMessageType;
    Eiger::EigerMessageParentType currentParentMessageType;
    int currentMessagePart;
    int numHeaderMessagesToExpect;
    rapidjson::Document jsonDocument;

    Eiger::FrameHeader currentHeader;

    static const std::string CONFIG_DETECTOR_MODEL;
    static const std::string DETECTOR_MODEL_500K;
    static const std::string DETECTOR_MODEL_1M;
    static const std::string DETECTOR_MODEL_4M;
    static const std::string DETECTOR_MODEL_9M;
    static const std::string DETECTOR_MODEL_16M;
  };

} /* namespace FrameReceiver */

#endif /* SRC_EIGERFRAMEDECODER_H_ */
