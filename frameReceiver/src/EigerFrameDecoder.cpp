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
                FrameDecoderZMQ(),
				current_frame_number_(-1),
        		current_frame_buffer_id_(-1),
        		current_frame_buffer_(0),
        		dropping_frame_data_(false),
        		frame_timeout_ms_(1000),
        		frames_timedout_(0),
				frames_dropped_(0),
				frames_allocated_(0),
				currentMessagePart(1),
				currentMessageType(Eiger::GLOBAL_HEADER_NONE),
				currentParentMessageType(Eiger::PARENT_MESSAGE_TYPE_GLOBAL),
				numHeaderMessagesToExpect(1)
{
    current_raw_buffer_.reset(new uint8_t[Eiger::raw_buffer_size]);
    dropped_frame_buffer_.reset(new uint8_t[Eiger::raw_buffer_size]);
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

void* EigerFrameDecoder::get_next_message_buffer(void)
{
	if ((currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_IMAGE_DATA && currentMessagePart == Eiger::image_data_blob_part) ||
		(currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_IMAGE_DATA && currentMessagePart == Eiger::image_data_appendix_part) ||
		(currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_GLOBAL && currentMessagePart == Eiger::global_detector_config_part) ||
		(currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_GLOBAL && currentMessagePart == Eiger::global_flatfield_data_part) ||
		(currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_GLOBAL && currentMessagePart == Eiger::global_mask_data_part) ||
		(currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_GLOBAL && currentMessagePart == Eiger::global_countrate_data_part) ||
		(currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_GLOBAL && currentMessagePart == Eiger::global_appendix_part)) {
	  allocate_next_frame_buffer();
	  return reinterpret_cast<void*>(static_cast<char*>(current_frame_buffer_)+sizeof(Eiger::FrameHeader));
	} else {
	  return reinterpret_cast<void*>(current_raw_buffer_.get());
	}
}

size_t EigerFrameDecoder::get_next_payload_size(void) const
{
    return Eiger::raw_buffer_size;
}

FrameDecoder::FrameReceiveState EigerFrameDecoder::process_message(size_t bytes_received)
{
	FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;

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
				currentParentMessageType = Eiger::PARENT_MESSAGE_TYPE_GLOBAL;
				// Reset the dropped frame count to start fresh for this acquisition
				frames_dropped_ = 0;
				frames_allocated_ = 0;
				// Get the series number from the message
				rapidjson::Value& seriesValue = jsonDocument[Eiger::SERIES_KEY.c_str()];
				currentHeader.series = seriesValue.GetInt();
				// Get the detail type to determine if there is more header to come
				rapidjson::Value& headerDetailValue = jsonDocument[Eiger::HEADER_DETAIL_KEY.c_str()];
				std::string hdetail(headerDetailValue.GetString());
				if (hdetail.compare(Eiger::HEADER_DETAIL_NONE) == 0) {
					currentMessageType = Eiger::GLOBAL_HEADER_NONE;
					LOG4CXX_TRACE(logger_, "Global header currentMessageType set to none");
					process_global_header_message(bytes_received);
					// Set to last message expected (1)
					numHeaderMessagesToExpect = Eiger::global_detector_none_part;
				} else if (hdetail.compare(Eiger::HEADER_DETAIL_BASIC) == 0) {
					// Current message type is config because 'Basic' includes the config message
					currentMessageType = Eiger::GLOBAL_HEADER_CONFIG;
					LOG4CXX_TRACE(logger_, "Global header currentMessageType set to config");
					// Set to last message expected (2)
					numHeaderMessagesToExpect = Eiger::global_detector_config_part;
				} else {
					// Current message type is config because 'All' includes the config message
					currentMessageType = Eiger::GLOBAL_HEADER_CONFIG;
					LOG4CXX_TRACE(logger_, "Global header currentMessageType set to config");
					// Set to last message expected (8)
					numHeaderMessagesToExpect = Eiger::global_countrate_data_part;
				}
				// Get the acquisition ID from the global header if it exists
				if (jsonDocument.HasMember(Eiger::ACQUISITION_ID_KEY.c_str()) == true) {
					rapidjson::Value& acqIDValue = jsonDocument[Eiger::ACQUISITION_ID_KEY.c_str()];
					std::string acqID(acqIDValue.GetString());
					strncpy(currentHeader.acquisitionID, acqID.c_str(), sizeof(currentHeader.acquisitionID));
				}

			} else if (htype.compare(Eiger::IMAGE_HEADER_TYPE) == 0) {
				currentParentMessageType = Eiger::PARENT_MESSAGE_TYPE_IMAGE_DATA;
				currentMessageType = Eiger::IMAGE_DATA;
				// Get the frame number from the message
				rapidjson::Value& frameValue = jsonDocument[Eiger::FRAME_KEY.c_str()];
				current_frame_number_ = frameValue.GetInt();
				currentHeader.frame_number = current_frame_number_;
				// Get the series number from the message
				rapidjson::Value& seriesValue = jsonDocument[Eiger::SERIES_KEY.c_str()];
				currentHeader.series = seriesValue.GetInt();
				// Get the hash value
				rapidjson::Value& hashValue = jsonDocument[Eiger::HASH_KEY.c_str()];
				std::string hash(hashValue.GetString());
				strncpy(currentHeader.hash, hash.c_str(), sizeof(currentHeader.hash));
				currentHeader.hash[sizeof(currentHeader.hash)-1] = '\0';

				// Get the acquisition ID from the global header if it exists
				if (jsonDocument.HasMember(Eiger::ACQUISITION_ID_KEY.c_str()) == true) {
					rapidjson::Value& acqIDValue = jsonDocument[Eiger::ACQUISITION_ID_KEY.c_str()];
					std::string acqID(acqIDValue.GetString());
					strncpy(currentHeader.acquisitionID, acqID.c_str(), sizeof(currentHeader.acquisitionID));
				}
			} else if (htype.compare(Eiger::END_HEADER_TYPE) == 0) {
				currentParentMessageType = Eiger::PARENT_MESSAGE_TYPE_END;
				currentMessageType = Eiger::END_OF_STREAM;
				// Get the series number from the message
				rapidjson::Value& seriesValue = jsonDocument[Eiger::SERIES_KEY.c_str()];
				currentHeader.series = seriesValue.GetInt();
				// Get the acquisition id from the message
				rapidjson::Value& acqIDValue = jsonDocument[Eiger::ACQUISITION_ID_KEY.c_str()];
				std::string acqID(acqIDValue.GetString());
				strncpy(currentHeader.acquisitionID, acqID.c_str(), sizeof(currentHeader.acquisitionID));
				process_end_message(bytes_received);
			} else {
				LOG4CXX_ERROR(logger_, std::string("Unknown header type ").append(htype));
			}
		}
	} else {
		if (currentMessageType == Eiger::IMAGE_DATA) {
			process_image_message(bytes_received);
		} else if (currentMessageType == Eiger::END_OF_STREAM) {
			LOG4CXX_ERROR(logger_, "Unexpected message at end of stream");
		} else {
			process_global_header_message(bytes_received);
		}
	}

	currentMessagePart++;

	// If we're on the global header message, set the current message part to the appendix if we have no more expected messages
	if (currentParentMessageType == Eiger::PARENT_MESSAGE_TYPE_GLOBAL) {
		if (currentMessagePart > numHeaderMessagesToExpect) {
			currentMessagePart = Eiger::global_appendix_part;
		}
	}

    return frame_state;
}

FrameDecoder::FrameReceiveState EigerFrameDecoder::process_global_header_message(size_t bytes_received) {
	FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;
	if (currentMessagePart == Eiger::global_detector_none_part) {
		// No buffer allocated, so allocate one
		allocate_next_frame_buffer();
		send_buffer();
	} else if (currentMessagePart == Eiger::global_detector_config_part) {
		currentHeader.data_size = bytes_received;
		send_buffer();
	} else if (currentMessagePart == Eiger::global_flatfield_header_part) {
		currentMessageType = Eiger::GLOBAL_HEADER_FLATFIELD;
		char temp_buffer[bytes_received+1];
		memcpy(temp_buffer, current_raw_buffer_.get(), bytes_received);
		temp_buffer[bytes_received] = '\0';
		jsonDocument.Parse(temp_buffer);
		// Get the shape
		rapidjson::Value& shapeValue = jsonDocument[Eiger::SHAPE_KEY.c_str()];
		if (shapeValue.IsArray()) {
			rapidjson::Value& s0Value = shapeValue[0];
			rapidjson::Value& s1Value = shapeValue[1];
			currentHeader.shapeSizeX = s0Value.GetInt();
			currentHeader.shapeSizeY = s1Value.GetInt();
			currentHeader.shapeSizeZ = 0;
		}
		// Get the data type
		rapidjson::Value& typeValue = jsonDocument[Eiger::DATA_TYPE_KEY.c_str()];
		std::string type(typeValue.GetString());
		strncpy(currentHeader.dataType, type.c_str(), sizeof(currentHeader.dataType));
		currentHeader.dataType[sizeof(currentHeader.dataType)-1] = '\0';
	} else if (currentMessagePart == Eiger::global_flatfield_data_part) {
		currentHeader.data_size = bytes_received;
		send_buffer();
	} else if (currentMessagePart == Eiger::global_mask_header_part) {
		currentMessageType = Eiger::GLOBAL_HEADER_MASK;
		char temp_buffer[bytes_received+1];
		memcpy(temp_buffer, current_raw_buffer_.get(), bytes_received);
		temp_buffer[bytes_received] = '\0';
		jsonDocument.Parse(temp_buffer);
		// Get the shape
		rapidjson::Value& shapeValue = jsonDocument[Eiger::SHAPE_KEY.c_str()];
		if (shapeValue.IsArray()) {
			rapidjson::Value& s0Value = shapeValue[0];
			rapidjson::Value& s1Value = shapeValue[1];
			currentHeader.shapeSizeX = s0Value.GetInt();
			currentHeader.shapeSizeY = s1Value.GetInt();
			currentHeader.shapeSizeZ = 0;
		}
		// Get the data type
		rapidjson::Value& typeValue = jsonDocument[Eiger::DATA_TYPE_KEY.c_str()];
		std::string type(typeValue.GetString());
		strncpy(currentHeader.dataType, type.c_str(), sizeof(currentHeader.dataType));
		currentHeader.dataType[sizeof(currentHeader.dataType)-1] = '\0';
	} else if (currentMessagePart == Eiger::global_mask_data_part) {
		currentHeader.data_size = bytes_received;
		send_buffer();
	} else if (currentMessagePart == Eiger::global_countrate_header_part) {
		currentMessageType = Eiger::GLOBAL_HEADER_COUNTRATE;
		char temp_buffer[bytes_received+1];
		memcpy(temp_buffer, current_raw_buffer_.get(), bytes_received);
		temp_buffer[bytes_received] = '\0';
		jsonDocument.Parse(temp_buffer);
		// Get the shape
		rapidjson::Value& shapeValue = jsonDocument[Eiger::SHAPE_KEY.c_str()];
		if (shapeValue.IsArray()) {
			rapidjson::Value& s0Value = shapeValue[0];
			rapidjson::Value& s1Value = shapeValue[1];
			currentHeader.shapeSizeX = s0Value.GetInt();
			currentHeader.shapeSizeY = s1Value.GetInt();
			currentHeader.shapeSizeZ = 0;
		}
		// Get the data type
		rapidjson::Value& typeValue = jsonDocument[Eiger::DATA_TYPE_KEY.c_str()];
		std::string type(typeValue.GetString());
		strncpy(currentHeader.dataType, type.c_str(), sizeof(currentHeader.dataType));
		currentHeader.dataType[sizeof(currentHeader.dataType)-1] = '\0';
	} else if (currentMessagePart == Eiger::global_countrate_data_part) {
		currentHeader.data_size = bytes_received;
		send_buffer();
	} else if (currentMessagePart == Eiger::global_appendix_part) {
		currentMessageType = Eiger::GLOBAL_HEADER_APPENDIX;
		currentHeader.data_size = bytes_received;
		send_buffer();
	}
    return frame_state;
}

FrameDecoder::FrameReceiveState EigerFrameDecoder::process_image_message(size_t bytes_received) {
	FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;
	if (currentMessagePart == Eiger::image_data_imaged_part) {
		// This is the message containing the image dimensions and encoding details
		char temp_buffer[bytes_received+1];
		memcpy(temp_buffer, current_raw_buffer_.get(), bytes_received);
		temp_buffer[bytes_received] = '\0';
		jsonDocument.Parse(temp_buffer);
		// Get the shape
		rapidjson::Value& shapeValue = jsonDocument[Eiger::SHAPE_KEY.c_str()];
		if (shapeValue.IsArray()) {
			size_t size = shapeValue.Capacity();
			if (size >= 2) {
				rapidjson::Value& s0Value = shapeValue[0];
				rapidjson::Value& s1Value = shapeValue[1];
				currentHeader.shapeSizeX = s0Value.GetInt();
				currentHeader.shapeSizeY = s1Value.GetInt();
				currentHeader.shapeSizeZ = 0;
			}
			if (size >= 3) {
				rapidjson::Value& s0Value = shapeValue[0];
				rapidjson::Value& s1Value = shapeValue[1];
				rapidjson::Value& s2Value = shapeValue[2];
				currentHeader.shapeSizeX = s0Value.GetInt();
				currentHeader.shapeSizeY = s1Value.GetInt();
				currentHeader.shapeSizeZ = s2Value.GetInt();
			}
		} else {
			LOG4CXX_ERROR(logger_, "Shape element wasn't an array");
		}
		// Get the data type
		rapidjson::Value& typeValue = jsonDocument[Eiger::DATA_TYPE_KEY.c_str()];
		std::string type(typeValue.GetString());
		strncpy(currentHeader.dataType, type.c_str(), sizeof(currentHeader.dataType));
		currentHeader.dataType[sizeof(currentHeader.dataType)-1] = '\0';
		// Get the encoding
		rapidjson::Value& encodingValue = jsonDocument[Eiger::ENCODING_KEY.c_str()];
		std::string encoding(encodingValue.GetString());
		strncpy(currentHeader.encoding, encoding.c_str(), sizeof(currentHeader.encoding));
		currentHeader.encoding[sizeof(currentHeader.encoding)-1] = '\0';
		// Get the size
		rapidjson::Value& sizeValue = jsonDocument[Eiger::SIZE_KEY.c_str()];
		currentHeader.size_in_header = sizeValue.GetInt64();
	} else if (currentMessagePart == Eiger::image_data_blob_part) {
		// This is the message containing the image blob
		currentHeader.data_size = bytes_received;

		frame_state = FrameDecoder::FrameReceiveStateComplete;
	} else if (currentMessagePart == Eiger::image_data_time_part) {
		// This is the message containing the image times
		char temp_buffer[bytes_received+1];
		memcpy(temp_buffer, current_raw_buffer_.get(), bytes_received);
		temp_buffer[bytes_received] = '\0';
		jsonDocument.Parse(temp_buffer);
		// Get the times
		rapidjson::Value& startValue = jsonDocument[Eiger::START_TIME_KEY.c_str()];
		currentHeader.startTime = startValue.GetInt64();
		rapidjson::Value& stopValue = jsonDocument[Eiger::STOP_TIME_KEY.c_str()];
		currentHeader.stopTime = stopValue.GetInt64();
		rapidjson::Value& realValue = jsonDocument[Eiger::REAL_TIME_KEY.c_str()];
		currentHeader.realTime = realValue.GetInt64();
		send_buffer();
		frame_state = FrameDecoder::FrameReceiveStateComplete;
	} else if (currentMessagePart == Eiger::image_data_appendix_part) {
		currentMessageType = Eiger::IMAGE_APPENDIX;
		currentHeader.data_size = bytes_received;
		send_buffer();
	}
    return frame_state;
}

FrameDecoder::FrameReceiveState EigerFrameDecoder::process_end_message(size_t bytes_received) {
	FrameDecoder::FrameReceiveState frame_state = FrameDecoder::FrameReceiveStateIncomplete;
	// No buffer allocated, so allocate one
	allocate_next_frame_buffer();
	send_buffer();
    return frame_state;
}

void EigerFrameDecoder::frame_meta_data(int meta)
{
	// EndOfFrame is the first bit of meta
	int eof = meta & 0x1;

	if (eof){
		currentMessagePart = 1; // Reset message part back to expect the first message part
		currentHeader.frame_number = -1; // Reset frame back to 0
		currentHeader.acquisitionID[0] = '\0'; // Reset the acquisition ID to empty
		currentHeader.series = 0; // Reset the series back to 0
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

    LOG4CXX_DEBUG_LEVEL(2, logger_, get_num_empty_buffers() << " empty buffers available. "
    		<< "Frames in the last acquisition: "
			<< frames_allocated_ << " allocated, "
			<< frames_dropped_ << " dropped");

}

unsigned int EigerFrameDecoder::elapsed_ms(struct timespec& start, struct timespec& end)
{

    double start_ns = ((double)start.tv_sec * 1000000000) + start.tv_nsec;
    double end_ns   = ((double)  end.tv_sec * 1000000000) +   end.tv_nsec;

    return (unsigned int)((end_ns - start_ns)/1000000);
}

void EigerFrameDecoder::allocate_next_frame_buffer(void) {
	if (current_frame_buffer_id_ == -1){
		if (empty_buffer_queue_.empty()){
			current_frame_buffer_ = dropped_frame_buffer_.get();
			if (!dropping_frame_data_){
				LOG4CXX_ERROR(logger_, "Frame data detected but no free buffers available. Dropping data for frame " << current_frame_number_);
				dropping_frame_data_ = true;
				frames_dropped_++;
			} else {
				LOG4CXX_ERROR(logger_, "Frame data detected but still no free buffers available. Dropping data for frame " << current_frame_number_);
				frames_dropped_++;
			}
		} else {
			current_frame_buffer_id_ = empty_buffer_queue_.front();
			empty_buffer_queue_.pop();
			//frame_buffer_map_[current_frame_seen_] = current_frame_buffer_id_;
			current_frame_buffer_ = buffer_manager_->get_buffer_address(current_frame_buffer_id_);
			dropping_frame_data_ = false;
			frames_allocated_++;
		}
	}
}

void EigerFrameDecoder::send_buffer(void) {

	if (!dropping_frame_data_) {
		currentHeader.messageType = currentMessageType;
		memcpy(current_frame_buffer_, &currentHeader, sizeof(Eiger::FrameHeader));

		// Notify main thread that frame is ready
		ready_callback_(current_frame_buffer_id_, current_frame_number_);

		// Reset current frame number ID so that if next frame has same number (e.g. repeated
		// sends of single frame 0), it is detected properly
		current_frame_number_ = -1;

		current_frame_buffer_id_ = -1;

		// Reset current frame header
		currentHeader.data_size = 0;
		currentHeader.shapeSizeX = 0;
		currentHeader.shapeSizeY = 0;
		currentHeader.shapeSizeZ = 0;
		currentHeader.startTime = 0;
		currentHeader.stopTime = 0;
		currentHeader.realTime = 0;
		currentHeader.size_in_header = 0;

		currentHeader.hash[0] = '\0';
		currentHeader.dataType[0] = '\0';
		currentHeader.encoding[0] = '\0';
	}
}

} /* namespace FrameReceiver */
