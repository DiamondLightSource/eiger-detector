/*
 * EigerProcessPlugin.cpp
 *
 *  Created on: 8 May 2017
 *      Authors: Matt Taylor, Gary Yendell
 */

#include "DebugLevelLogger.h"
#include "Json.h"
#include "TmpfsFrame.h"
#include "WrapperFrame.h"

#include "EigerDefinitions.h"
#include "EigerProcessPlugin.h"

namespace FrameProcessor
{

const std::string EigerProcessPlugin::CONFIG_ENDPOINT = "endpoint";
const std::string EigerProcessPlugin::CONFIG_PERSISTENT_FILES = "persistent_files";

  /**
   * Constuctor
   */
  EigerProcessPlugin::EigerProcessPlugin() :
    zmq_context_(),
    zmq_socket_(zmq_context_, ZMQ_PULL),
    persistent_files_(false)
  {
    // Setup logging for the class
    logger_ = Logger::getLogger("FP.EigerProcessPlugin");
    logger_->setLevel(Level::getAll());

    int hwm = 10000;
    zmq_socket_.setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));

    LOG4CXX_TRACE(logger_, "EigerProcessPlugin constructor.");
  }

  /**
   * Destructor
   */
  EigerProcessPlugin::~EigerProcessPlugin()
  {
    rx_thread_->join();
    rx_thread_.reset();
  }

  /** Handle configuration requests
   *
   * @param config - Configuration message
   * @param reply - Reply message that will be sent back to client
   */
  void EigerProcessPlugin::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply)
  {
    // Protect this method
    boost::lock_guard<boost::recursive_mutex> lock(mutex_);

    if (config.has_param(EigerProcessPlugin::CONFIG_ENDPOINT) && this->endpoint_.empty()) {
      this->endpoint_ = config.get_param<std::string>(EigerProcessPlugin::CONFIG_ENDPOINT);

      try {
        this->zmq_socket_.connect(this->endpoint_.c_str());
      }
      catch (zmq::error_t& e) {
        LOG4CXX_ERROR(logger_, "Failed to connect to " << this->endpoint_ << ": " << e.what());
        return;
      }

      rx_thread_ = boost::shared_ptr<boost::thread>(
        new boost::thread(boost::bind(&EigerProcessPlugin::handle_rx_socket, this))
      );
    }

    if (config.has_param(EigerProcessPlugin::CONFIG_PERSISTENT_FILES)) {
      this->persistent_files_ = config.get_param<bool>(EigerProcessPlugin::CONFIG_PERSISTENT_FILES);
      if (this->persistent_files_) {
        LOG4CXX_INFO(logger_, "Persistent files enabled");
      } else {
        LOG4CXX_INFO(logger_, "Persistent files disabled");
      }
    }
  }

  /** Listen on ZMQ channel for detector data
   *
   */
  void EigerProcessPlugin::handle_rx_socket()
  {
    LOG4CXX_INFO(logger_, "Connected to " << this->endpoint_ << " - Listening...");

    zmq::message_t buffer_message;
    zmq::pollitem_t items [] = {{this->zmq_socket_, 0, ZMQ_POLLIN, 0}};
    while (this->isWorking()) {
      // Poll for 1000ms and skip loop if no messages were received (to check for shutdown)
      zmq::poll(&items[0], 1, 1000);
      if (!(items[0].revents & ZMQ_POLLIN)) {
        continue;
      }

      // Message found on socket

      zmq_socket_.recv(&buffer_message);
      LOG4CXX_DEBUG_LEVEL(1, logger_, "Received data message");

      struct stream2_msg* message;
      const uint8_t* cbor_buffer_ptr = (const uint8_t*) buffer_message.data();
      stream2_result error = stream2_parse_msg(cbor_buffer_ptr, buffer_message.size(), &message);
      if (error) {
        LOG4CXX_ERROR(logger_, "stream2_parse_msg returned error code " << (int) error);
        continue;
      }

      switch (message->type) {
        case STREAM2_MSG_START:
          handle_start_msg((struct stream2_start_msg*) message, buffer_message);
          break;
        case STREAM2_MSG_IMAGE:
          handle_image_msg((struct stream2_image_msg*) message, buffer_message);
          break;
        case STREAM2_MSG_END:
          handle_end_msg((struct stream2_end_msg*) message, buffer_message);
          break;
      }

      stream2_free_msg(message);
    }

    LOG4CXX_INFO(logger_, "Shutting down");
  }

  /**
   * Handle a start message
   *
   * \param[in] message Parsed stream2 start message
   * \param[in] zmq_message ZMQ message containg raw cbor data blob
   */
  void EigerProcessPlugin::handle_start_msg(struct stream2_start_msg* message, const zmq::message_t& zmq_message) {
    LOG4CXX_INFO(logger_, "Handling start message");
    this->current_series_id_ = message->series_id;

    OdinData::JsonDict json;
    json.add("series_id", message->series_id);
    publish_meta(get_name(), "eiger-start", zmq_message.data(), zmq_message.size(), json.str());
  }

  /**
   * Handle an image message
   *
   * \param[in] message Parsed stream2 image message
   * \param[in] frame Frame to process
   */
  void EigerProcessPlugin::handle_image_msg(struct stream2_image_msg* message, const zmq::message_t& zmq_message) {
    LOG4CXX_TRACE(logger_, "Processing series " << message->series_id << " image " << message->image_id);

    if (message->series_id != this->current_series_id_) {
      std::stringstream ss;
      ss << "Received image message with series_number " << message->series_id;
      ss << " - expecting " << this->current_series_id_;
      ss << " - ignoring";
      this->set_error(ss.str());
      return;
    }

    // Create file path from series and image id, nesting blocks of 1000 files in subdirectories
    std::string block = std::to_string((int) message->image_id / 1000);
    std::stringstream file_path_stream;
    file_path_stream << "/dev/shm/eiger/";
    if (this->persistent_files_) {
      // If files are persistent then store by series id
      file_path_stream << message->series_id << "/";
    }
    file_path_stream << block << "/" << message->image_id;
    std::string file_path = file_path_stream.str();

    // Create a Frame backed by a file in /dev/shm and copy zmq_message data into it
    FrameMetaData frame_meta_data;
    boost::shared_ptr<Frame> frame;
    try {
      frame = boost::shared_ptr<Frame>(
        new TmpfsFrame(
          file_path,
          frame_meta_data,
          zmq_message.data(),
          zmq_message.size(),
          0,
          !this->persistent_files_
        )
      );
    } catch (std::runtime_error& e) {
      LOG4CXX_ERROR(logger_, "Failed to create TmpfsFrame" << ": " << e.what());
      return;
    }

    // Parse out fields constant for all image channels from first channel
    const stream2_image_data* image_data = &message->data.ptr[0];
    DataType data_type = get_stream2_data_type(image_data);
    dimensions_t dimensions = get_stream2_dimensions(image_data);
    CompressionType compression = get_stream2_compression(image_data);

    // Construct messages to be published on meta data channel
    OdinData::JsonDict header;
    header.add("series_id", message->series_id);
    OdinData::JsonDict data;
    data.add("series_id", message->series_id);
    data.add("image_id", message->image_id);
    // First element of *_time is the actual time, second element is the time base frequency
    data.add("start_time", message->start_time[0]);
    data.add("stop_time", message->stop_time[0]);
    data.add("real_time", message->real_time[0]);
    data.add("type", get_type_from_enum(data_type));
    data.add("shape", dimensions);
    data.add("compression", get_compress_from_enum(compression));
    std::vector<uint32_t> compressed_size;  // To be populated from each image channel

    frame_meta_data.set_data_type(data_type);
    frame_meta_data.set_dimensions(dimensions);
    frame_meta_data.set_compression_type(compression);
    frame_meta_data.set_frame_number(message->image_id);
    // Just use the first channel size for parameter dataset
    frame_meta_data.set_parameter("compressed_size", get_stream2_compressed_size(image_data));

    // Handle each channel by wrapping the original Frame
    for (size_t data_idx = 0; data_idx < message->data.len; data_idx++) {
      image_data = &message->data.ptr[data_idx];
      frame->set_image_size(image_data->data.array.data.len);

      // Create a wrapper frame with an offset to the image channel
      boost::shared_ptr<WrapperFrame> wrapper_frame = boost::shared_ptr<WrapperFrame>(
        new WrapperFrame(
          frame,
          (const uint8_t*) image_data->data.array.data.ptr - (const uint8_t*) zmq_message.data()
        )
      );

      // Update name and size for this image channel
      std::stringstream dataset_name_stream;
      dataset_name_stream << "data";
      // Dataset names are `data`, `data2`, `data3`, ...
      if (data_idx > 0) {
        dataset_name_stream << data_idx + 1;
      }
      frame_meta_data.set_dataset_name(dataset_name_stream.str());
      compressed_size.push_back(get_stream2_compressed_size(image_data));
      wrapper_frame->set_meta_data(frame_meta_data);

      this->push(wrapper_frame);
    }

    data.add("size", compressed_size);
    publish_meta(get_name(), "eiger-image", data.str(), header.str());

    LOG4CXX_TRACE(logger_, "Finished handling image message");
  }

  /**
   * Handle an end message
   *
   * \param[in] message Parsed stream2 end message
   * \param[in] zmq_message ZMQ message containg raw cbor data blob
   */
  void EigerProcessPlugin::handle_end_msg(struct stream2_end_msg* message, const zmq::message_t& zmq_message) {
    // this->notify_end_of_acquisition();  // Need to delay this or we could miss the last few frames
    LOG4CXX_INFO(logger_, "Handling end message");

    OdinData::JsonDict json;
    json.add("series_id", message->series_id);
    publish_meta(get_name(), "eiger-end", zmq_message.data(), zmq_message.size(), json.str());
  }

  void EigerProcessPlugin::process_frame(boost::shared_ptr<Frame> frame)
  {
    LOG4CXX_ERROR(logger_, "EigerProcessPlugin::process_frame should not be called");
  }

  /**
   * Get the compression algorithm from a stream2_image_data
   *
   * \param[in] image_data The stream2_image_data to extract compression from
   */
  CompressionType EigerProcessPlugin::get_stream2_compression(const struct stream2_image_data* image_data) {
    char* compression = image_data->data.array.data.compression.algorithm;
    if (strcmp(compression, "bslz4") == 0) {
        return CompressionType::bslz4;
    }
    else if (strcmp(compression, "lz4") == 0) {
        return CompressionType::lz4;
    } else {
        return CompressionType::no_compression;
    }
  }

  /**
   * Get the compressed size from a stream2_image_data
   *
   * \param[in] image_data The stream2_image_data to extract compression from
   *
   * \return The compressed size in bytes
   */
  uint32_t EigerProcessPlugin::get_stream2_compressed_size(const struct stream2_image_data* image_data) {
    return image_data->data.array.data.len;
  }

  /**
   * Get the data type from a stream2_image_data
   *
   * \param[in] image_data The stream2_image_data to extract data type from
   *
   * \return The data type enum
   */
  DataType EigerProcessPlugin::get_stream2_data_type(const struct stream2_image_data* image_data) {
    uint64_t elem_size;
    stream2_typed_array_elem_size(&image_data->data.array, &elem_size);
    if (elem_size == 1) {
      return DataType::raw_8bit;
    } else if (elem_size == 2) {
      return DataType::raw_16bit;
    } else if (elem_size == 4) {
      return DataType::raw_32bit;
    } else {
      LOG4CXX_ERROR(logger_, "Unknown frame data type size: " << elem_size);
      return DataType::raw_unknown;
    }
  }

  /**
   * Get the dimenstions from a stream2_image_data
   *
   * \param[in] image_data The stream2_image_data to extract dimensions from
   *
   * \return The data type enum
   */
  dimensions_t EigerProcessPlugin::get_stream2_dimensions(const struct stream2_image_data* image_data) {
    dimensions_t dims;
    dims.push_back(image_data->data.dim[0]);
    dims.push_back(image_data->data.dim[1]);

    return dims;
  }

  /** Provide configuration readback
   *
   * @param reply - Response IpcMessage.
   */
  void EigerProcessPlugin::requestConfiguration(OdinData::IpcMessage& reply)
  {
    reply.set_param(this->get_name() + "/" + EigerProcessPlugin::CONFIG_ENDPOINT, this->endpoint_);
  }

  int EigerProcessPlugin::get_version_major() {
    return EIGER_DETECTOR_VERSION_MAJOR;
  }

  int EigerProcessPlugin::get_version_minor() {
    return EIGER_DETECTOR_VERSION_MINOR;
  }

  int EigerProcessPlugin::get_version_patch() {
    return EIGER_DETECTOR_VERSION_PATCH;
  }

  std::string EigerProcessPlugin::get_version_short() {
    return EIGER_DETECTOR_VERSION_STR_SHORT;
  }

  std::string EigerProcessPlugin::get_version_long() {
    return EIGER_DETECTOR_VERSION_STR;
  }

} /* namespace FrameProcessor */
