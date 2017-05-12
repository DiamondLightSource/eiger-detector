/*
 * EigerFrameDecoder.h
 *
 *  Created on: 23 Feb 2017
 *      Author: gnx91527
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
	void init(LoggerPtr& logger, bool enable_packet_logging=false, unsigned int frame_timeout_ms=1000);
    const size_t get_frame_buffer_size(void) const;
    const size_t get_frame_header_size(void) const;

    void* get_next_message_buffer(void);
    size_t get_next_payload_size(void) const;
    FrameDecoder::FrameReceiveState process_message(size_t bytes_received);
    void frame_meta_data(int meta);

    void monitor_buffers(void);

    void* get_packet_header_buffer(void);

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

    uint32_t current_frame_number_;
    int current_frame_buffer_id_;

    bool dropping_frame_data_;

    unsigned int frame_timeout_ms_;
    unsigned int frames_timedout_;

    Eiger::EigerMessageType currentMessageType;
    Eiger::EigerMessageParentType currentParentMessageType;
    int currentMessagePart;
    rapidjson::Document jsonDocument;

    Eiger::FrameHeader currentHeader;
};

} /* namespace FrameReceiver */

#endif /* SRC_EIGERFRAMEDECODER_H_ */
