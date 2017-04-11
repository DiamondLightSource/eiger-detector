/*
 * EigerFrameDecoder.cpp
 *
 *  Created on: 23 Feb 2017
 *      Author: gnx91527
 */

#include "EigerFrameDecoder.h"

namespace FrameReceiver
{

EigerFrameDecoder::EigerFrameDecoder() :
                FrameDecoder(),
        		current_frame_seen_(-1),
        		current_frame_buffer_id_(-1),
        		current_frame_buffer_(0),
        		dropping_frame_data_(false),
        		frame_timeout_ms_(1000),
        		frames_timedout_(0)
{
    current_raw_buffer_.reset(new uint8_t[Eiger::raw_buffer_size]);
}

void EigerFrameDecoder::init(LoggerPtr& logger, bool enable_packet_logging, unsigned int frame_timeout_ms)
{
	FrameDecoder::init(logger, enable_packet_logging, frame_timeout_ms);

    if (enable_packet_logging_) {
        LOG4CXX_INFO(packet_logger_, "Logging incoming data");
    }
}

EigerFrameDecoder::~EigerFrameDecoder()
{
}

const size_t EigerFrameDecoder::get_frame_buffer_size(void) const
{
    return Eiger::frame_size;
}

const size_t EigerFrameDecoder::get_frame_header_size(void) const
{
    return 0;
}

const size_t EigerFrameDecoder::get_packet_header_size(void) const
{
    return 0;
}

void* EigerFrameDecoder::get_packet_header_buffer(void)
{
    return NULL;
}

void EigerFrameDecoder::process_packet_header(size_t bytes_received, int port, struct sockaddr_in* from_addr)
{
  // NO OP
}

void* EigerFrameDecoder::get_next_payload_buffer(void) const
{
    return reinterpret_cast<void*>(current_frame_buffer_);
}

size_t EigerFrameDecoder::get_next_payload_size(void) const
{
    return Eiger::raw_buffer_size;
}

FrameDecoder::FrameReceiveState EigerFrameDecoder::process_packet(size_t bytes_received)
{
	if (current_frame_buffer_id_ == -1){
		if (empty_buffer_queue_.empty()){
			current_frame_buffer_ = dropped_frame_buffer_.get();
			if (!dropping_frame_data_){
				LOG4CXX_ERROR(logger_, "Frame data detected but no free buffers available. Dropping packet data for this frame");
				dropping_frame_data_ = true;
			}
		} else {
			current_frame_buffer_id_ = empty_buffer_queue_.front();
			empty_buffer_queue_.pop();
			//frame_buffer_map_[current_frame_seen_] = current_frame_buffer_id_;
			current_frame_buffer_ = buffer_manager_->get_buffer_address(current_frame_buffer_id_);
		}
	}

    FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;

    // Copy the contents from the current_raw_buffer to the current_frame_buffer
    memcpy(current_frame_buffer_, current_raw_buffer_.get(), bytes_received);

    return frame_state;
}

FrameDecoder::FrameReceiveState EigerFrameDecoder::process_eof(bool eof)
{
	FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;

	if (eof){
		// TODO: Matthew do your stuff here.  Frame ptr is buffer_manager_->get_buffer_address(current_frame_buffer_id_);
		char *fPtr = reinterpret_cast<char*>(buffer_manager_->get_buffer_address(current_frame_buffer_id_));
		LOG4CXX_ERROR(logger_, "FULL FRAME: " << fPtr);


	    // Set frame state accordingly
		frame_state = FrameDecoder::FrameReceiveStateComplete;

		if (!dropping_frame_data_){
			// Erase frame from buffer map
			//frame_buffer_map_.erase(current_frame_seen_);

			// Notify main thread that frame is ready
			ready_callback_(current_frame_buffer_id_, current_frame_seen_);

			// Reset current frame seen ID so that if next frame has same number (e.g. repeated
			// sends of single frame 0), it is detected properly
			//current_frame_seen_ = -1;

			current_frame_buffer_id_ = -1;
		}
	}

	return frame_state;
}

void EigerFrameDecoder::monitor_buffers(void)
{

    int frames_timedout = 0;
    struct timespec current_time;

    gettime(&current_time);

    // Loop over frame buffers currently in map and check their state
    std::map<uint32_t, int>::iterator buffer_map_iter = frame_buffer_map_.begin();
    while (buffer_map_iter != frame_buffer_map_.end())
    {
        uint32_t frame_num = buffer_map_iter->first;
        int      buffer_id = buffer_map_iter->second;
        void*    buffer_addr = buffer_manager_->get_buffer_address(buffer_id);
//        LATRD::FrameHeader* frame_header = reinterpret_cast<LATRD::FrameHeader*>(buffer_addr);

//        if (elapsed_ms(frame_header->frame_start_time, current_time) > frame_timeout_ms_)
//        {
//            LOG4CXX_DEBUG_LEVEL(1, logger_, "Frame " << frame_num << " in buffer " << buffer_id
//                    << " addr 0x" << std::hex << buffer_addr << std::dec
//                    << " timed out with " << frame_header->packets_received << " packets received");
//
//            frame_header->frame_state = FrameReceiveStateTimedout;
//            ready_callback_(buffer_id, frame_num);
//            frames_timedout++;
//
//            frame_buffer_map_.erase(buffer_map_iter++);
//        }
//        else
//        {
            buffer_map_iter++;
//        }
    }
    if (frames_timedout)
    {
        LOG4CXX_WARN(logger_, "Released " << frames_timedout << " timed out incomplete frames");
    }
    frames_timedout_ += frames_timedout;

//    LOG4CXX_DEBUG_LEVEL(2, logger_, get_num_mapped_buffers() << " frame buffers in use, "
//            << get_num_empty_buffers() << " empty buffers available, "
//            << frames_timedout_ << " incomplete frames timed out");

}

uint32_t EigerFrameDecoder::get_frame_number(void) const
{
    uint32_t frame_number_raw = 0; //*(reinterpret_cast<uint32_t*>(raw_packet_header()+LATRD::frame_number_offset));
    return ntohl(frame_number_raw);
}

unsigned int EigerFrameDecoder::elapsed_ms(struct timespec& start, struct timespec& end)
{

    double start_ns = ((double)start.tv_sec * 1000000000) + start.tv_nsec;
    double end_ns   = ((double)  end.tv_sec * 1000000000) +   end.tv_nsec;

    return (unsigned int)((end_ns - start_ns)/1000000);
}

} /* namespace FrameReceiver */
