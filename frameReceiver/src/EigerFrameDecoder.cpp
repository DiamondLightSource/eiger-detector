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
				current_frame_number_(-1),
        		current_frame_buffer_id_(-1),
        		current_frame_buffer_(0),
        		dropping_frame_data_(false),
        		frame_timeout_ms_(1000),
        		frames_timedout_(0),
				currentMessagePart(1),
				currentMessageType(Eiger::GLOBAL_HEADER)
{
    current_raw_buffer_.reset(new uint8_t[Eiger::raw_buffer_size]);
}

void EigerFrameDecoder::init(LoggerPtr& logger, bool enable_packet_logging, unsigned int frame_timeout_ms)
{
	FrameDecoder::init(logger, enable_packet_logging, frame_timeout_ms);
	LOG4CXX_INFO(logger_, "Eiger frame decoder init called");

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
    return reinterpret_cast<void*>(current_raw_buffer_.get());
}

size_t EigerFrameDecoder::get_next_payload_size(void) const
{
    return Eiger::raw_buffer_size;
}

FrameDecoder::FrameReceiveState EigerFrameDecoder::process_packet(size_t bytes_received)
{
	FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;

    //temp_buffer[bytes_received] = '\0';
	//LOG4CXX_ERROR(logger_, "Process packet called.  Buffer contains [" << temp_buffer << "]");

	// If on first message part, parse the message to find out what type of message it is
	if (currentMessagePart == 1) {
		char temp_buffer[bytes_received+1];
		memcpy(temp_buffer, current_raw_buffer_.get(), bytes_received);
		temp_buffer[bytes_received] = '\0';
		jsonDocument.Parse(temp_buffer);
			if (jsonDocument.HasParseError()) {
				LOG4CXX_ERROR(logger_, "Error parsing stream message into json");
			} else {
				rapidjson::Value& headerTypeValue = jsonDocument[Eiger::HEADER_TYPE_KEY.c_str()];
				std::string htype(headerTypeValue.GetString());
				if (htype.compare(Eiger::GLOBAL_HEADER_TYPE) == 0) {
					currentMessageType = Eiger::GLOBAL_HEADER;
				} else if (htype.compare(Eiger::IMAGE_HEADER_TYPE) == 0) {
					currentMessageType = Eiger::IMAGE_DATA;
					// Get the frame number from the message
					rapidjson::Value& seriesValue = jsonDocument[Eiger::FRAME_KEY.c_str()];
					current_frame_number_ = seriesValue.GetInt();
					currentHeader.frame_number = current_frame_number_;
				} else if (htype.compare(Eiger::END_HEADER_TYPE) == 0) {
					currentMessageType = Eiger::END_OF_STREAM;
				} else {
					LOG4CXX_ERROR(logger_, std::string("Unknown header type ").append(htype));
				}
			}
	} else {
		if (currentMessageType == Eiger::IMAGE_DATA && currentMessagePart == Eiger::image_data_imaged_part) {
			// This is the message containing the image dimensions and encoding details
			// TODO parse out dimensions and encoding info
			currentHeader.shapeSizeX = 2070;
			currentHeader.shapeSizeY = 2167;
			currentHeader.shapeSizeZ = 0;
			LOG4CXX_DEBUG(logger_, "Copied message part that was image dimensions");
		} else if (currentMessageType == Eiger::IMAGE_DATA && currentMessagePart == Eiger::image_data_blob_part) {
			// This is the message containing the image bloc
			// Copy the contents from the current_raw_buffer to the current_frame_buffer
			currentHeader.blob_size = bytes_received;

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


			memcpy(current_frame_buffer_, &currentHeader, sizeof(Eiger::FrameHeader));
			void* dataPtr = static_cast<char*>(current_frame_buffer_)+sizeof(Eiger::FrameHeader);
			memcpy(dataPtr, current_raw_buffer_.get(), bytes_received);

			LOG4CXX_DEBUG(logger_, "Copied message part that was image blob and header data");
			frame_state = FrameDecoder::FrameReceiveStateComplete;
		}
	}

	currentMessagePart++;

    return frame_state;
}

void EigerFrameDecoder::frame_meta_data(int meta)
{
	// EndOfFrame is the first bit of meta
	int eof = meta & 0x1;
	LOG4CXX_DEBUG_LEVEL(1, logger_, "frame_meta_data called with eof " << eof);

	if (eof){
		if (!dropping_frame_data_){
			// TODO: Matthew do your stuff here.  Frame ptr is buffer_manager_->get_buffer_address(current_frame_buffer_id_);
			//char *fPtr = reinterpret_cast<char*>(buffer_manager_->get_buffer_address(current_frame_buffer_id_));
			//LOG4CXX_ERROR(logger_, "FULL FRAME: " << fPtr);

			// Erase frame from buffer map
			//frame_buffer_map_.erase(current_frame_seen_);

			if (currentMessageType == Eiger::IMAGE_DATA) {
				// Notify main thread that frame is ready
				LOG4CXX_DEBUG(logger_, "Notified of frame num " << current_frame_number_);
				ready_callback_(current_frame_buffer_id_, current_frame_number_);
			}

			// Reset current frame number ID so that if next frame has same number (e.g. repeated
			// sends of single frame 0), it is detected properly
			current_frame_number_ = -1;

			current_frame_buffer_id_ = -1;
		}
		currentMessagePart = 1; // Reset message part back to expect the first message part
	}
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

    LOG4CXX_DEBUG_LEVEL(2, logger_, get_num_mapped_buffers() << " frame buffers in use, "
            << get_num_empty_buffers() << " empty buffers available, "
            << frames_timedout_ << " incomplete frames timed out");

}

unsigned int EigerFrameDecoder::elapsed_ms(struct timespec& start, struct timespec& end)
{

    double start_ns = ((double)start.tv_sec * 1000000000) + start.tv_nsec;
    double end_ns   = ((double)  end.tv_sec * 1000000000) +   end.tv_nsec;

    return (unsigned int)((end_ns - start_ns)/1000000);
}

} /* namespace FrameReceiver */
