/*
 * EigerProcessPlugin.cpp
 *
 *  Created on: 8 May 2017
 *      Authors: Matt Taylor, Gary Yendell
 */
#include <memory_resource>

#include "DebugLevelLogger.h"
#include "Json.h"
#include "WrapperFrame.h"
#include "DataBlockFrame.h"

#include "EigerDefinitions.h"
#include "EigerProcessPlugin.h"

namespace FrameProcessor
{
  /**
   * Constuctor
   */
  EigerProcessPlugin::EigerProcessPlugin() : zmq_context_(),
                                             zmq_socket_(zmq_context_, ZMQ_PULL),
                                             dropped_frames_(0),
                                             buffer_(nullptr, BODY_CH_SIZE),
                                             writer_(buffer_)
  {
    // Setup logging for the class
    // logger_->setLevel(Level::getAll());

    int hwm = 10000;
    zmq_socket_.setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));
    add_config_param_metadata(EigerProcessPlugin::CONFIG_ENDPOINT, PMDD::STRING_T, PMDA::READ_WRITE);
    add_status_param_metadata(EigerProcessPlugin::STATUS_DROPPED_FRAMES, PMDD::UINT_T, PMDA::READ_ONLY, 0);
    LOG4CXX_TRACE(Logger::getLogger("FP.EigerProcessPlugin"), "EigerProcessPlugin constructor.");
  }

  /**
   * Destructor
   */
  EigerProcessPlugin::~EigerProcessPlugin()
  {
    rx_thread_.join();
  }

  /** Handle configuration requests
   *
   * @param config - Configuration message
   * @param reply - Reply message that will be sent back to client
   */
  void EigerProcessPlugin::configure(OdinData::IpcMessage &config, OdinData::IpcMessage &reply)
  {
    if (config.has_param(EigerProcessPlugin::CONFIG_OFFSET_PLUGIN)) {
      this->offset_index_ = config.get_param<std::string>(EigerProcessPlugin::CONFIG_OFFSET_PLUGIN);
    }

    if (config.has_param(EigerProcessPlugin::CONFIG_RAWFW_PLUGIN)) {
      this->rawfilewriter_index_ = config.get_param<std::string>(EigerProcessPlugin::CONFIG_RAWFW_PLUGIN);
    }

    if (config.has_param(EigerProcessPlugin::CONFIG_ENDPOINT) && this->endpoint_.empty())
    {
      this->endpoint_ = config.get_param<std::string>(EigerProcessPlugin::CONFIG_ENDPOINT);
      try
      {
        this->zmq_socket_.connect(this->endpoint_.c_str());
      }
      catch (zmq::error_t &e)
      {
        LOG4CXX_ERROR(Logger::getLogger("FP.EigerProcessPlugin"), "Failed to connect to " << this->endpoint_ << ": " << e.what());
        return;
      }

      rx_thread_ = std::thread([this]() { this->handle_rx_socket(); });
    }
  }

  /** Listen on ZMQ channel for detector data
   *
   */
  void EigerProcessPlugin::handle_rx_socket()
  {
    LOG4CXX_INFO(Logger::getLogger("FP.EigerProcessPlugin"), "Connected to " << this->endpoint_ << " - Listening...");

    zmq::message_t buffer_message;
    zmq::pollitem_t items[] = {{this->zmq_socket_, 0, ZMQ_POLLIN, 0}};
    while (this->isWorking())
    {
      // Poll for 1000ms and skip loop if no messages were received (to check for shutdown)
      zmq::poll(&items[0], 1, 1000);
      if (!(items[0].revents & ZMQ_POLLIN))
      {
        continue;
      }

      // Message found on socket

      zmq_socket_.recv(&buffer_message);
      LOG4CXX_DEBUG_LEVEL(1, Logger::getLogger("FP.EigerProcessPlugin"), "Received data message");

      struct stream2_msg *message;
      const uint8_t *cbor_buffer_ptr = (const uint8_t *)buffer_message.data();
      stream2_result error = stream2_parse_msg(cbor_buffer_ptr, buffer_message.size(), &message);
      if (error)
      {
        LOG4CXX_ERROR(Logger::getLogger("FP.EigerProcessPlugin"), "stream2_parse_msg returned error code " << (int)error);
        continue;
      }

      switch (message->type)
      {
      case STREAM2_MSG_START:
        handle_start_msg((struct stream2_start_msg *)message, buffer_message);
        break;
      case STREAM2_MSG_IMAGE:
        handle_image_msg((struct stream2_image_msg *)message, buffer_message);
        break;
      case STREAM2_MSG_END:
        handle_end_msg((struct stream2_end_msg *)message, buffer_message);
        break;
      }

      stream2_free_msg(message);
    }
    LOG4CXX_INFO(Logger::getLogger("FP.EigerProcessPlugin"), "Shutting down");
  }

  /**
   * Handle a start message
   *
   * \param[in] message Parsed stream2 start message
   * \param[in] zmq_message ZMQ message containg raw cbor data blob
   */
  void EigerProcessPlugin::handle_start_msg(struct stream2_start_msg *message, const zmq::message_t &zmq_message)
  {
    LOG4CXX_INFO(Logger::getLogger("FP.EigerProcessPlugin"), "Handling start message");
    this->current_series_id_ = message->series_id;
    char to_str[42] = {'\0'};
    snprintf(to_str, sizeof(to_str), "{\"series_id\": %lu}",  message->series_id);
    publish_meta(get_name(), "eiger-start", zmq_message.data(), zmq_message.size(), to_str);
  }

  /**
   * Handle an image message
   *
   * \param[in] message Parsed stream2 image message
   * \param[in] frame Frame to process
   */
  void EigerProcessPlugin::handle_image_msg(struct stream2_image_msg *message, const zmq::message_t &zmq_message)
  {
    LOG4CXX_TRACE(Logger::getLogger("FP.EigerProcessPlugin"), "Processing series " 
                  << message->series_id << " image " << message->image_id);
    char to_str[128] = {'\0'};
    if (message->series_id != this->current_series_id_) {
      snprintf(to_str, sizeof(to_str), "Series number mimatch %lu - expecting %lu - ignoring",
               message->series_id, this->current_series_id_);
      this->set_error(to_str);
      return;
    }

    // Parse out fields constant for all image channels from first channel
    const stream2_image_data *image_data = &message->data.ptr[0];
    snprintf(to_str, sizeof(to_str), "%lu", message->image_id); // acquisition ID
    DataType data_type = get_stream2_data_type(image_data);
    auto [compression_type, compression_str] = get_stream2_compression(image_data->data.array.data.compression.algorithm);
    // Create a Frame and send it to the RawFileWriterPlugin
    FrameMetaData frame_meta_data{static_cast<long long>(message->image_id), "", data_type, to_str, 
                                  {image_data->data.dim[0], image_data->data.dim[1]}, compression_type};
    // Just use the first channel size for parameter dataset
    frame_meta_data.set_parameter("compressed_size", image_data->data.array.data.len);
    boost::shared_ptr<DataBlockFrame> frame;
    try
    {
      frame = boost::make_shared<DataBlockFrame>(
              DataBlockFrame(
                frame_meta_data,
                zmq_message.data(),
                zmq_message.size(),
                0));

      // call RawFileWriterPlugin index here
      this->push(this->rawfilewriter_index_, frame);
    }
    catch (std::runtime_error &e)
    {
      ++this->dropped_frames_;
      LOG4CXX_ERROR(Logger::getLogger("FP.EigerProcessPlugin"), "Failed to create DataBlockFrame: " << e.what());
      return;
    }
    buffer_.Clear();
    writer_.Reset(buffer_);
    writer_.StartObject();
    writer_.Key("series_id");
    writer_.Uint64(message->series_id);
    writer_.Key("image_id");
    writer_.Uint64(message->image_id);
    writer_.Key("start_time");
    writer_.Uint64(message->start_time[0]);
    writer_.Key("stop_time");
    writer_.Uint64(message->stop_time[0]);
    writer_.Key("real_time");
    writer_.Uint64(message->real_time[0]);
    writer_.Key("type");
    writer_.String(get_type_from_enum(data_type).c_str());
    writer_.Key("shape");
    writer_.StartArray();
    writer_.Uint64(image_data->data.dim[0]);
    writer_.Uint64(image_data->data.dim[1]);
    writer_.EndArray();
    writer_.Key("compression");
    writer_.String(compression_str);

    size_t approx_block_size = (sizeof(WrapperFrame) + 32) * message->data.len;
    std::vector<std::byte> raw_buffer(approx_block_size);
    std::pmr::monotonic_buffer_resource mem_pool(raw_buffer.data(), raw_buffer.size());
    std::pmr::polymorphic_allocator<WrapperFrame> alloc(&mem_pool);
    // Handle each channel by wrapping the original Frame
    for (size_t data_idx = 0; data_idx < message->data.len; ++data_idx) {
      image_data = &message->data.ptr[data_idx];
      frame->set_image_size(image_data->data.array.data.len);
      // Update name and size for this image channel
      // Dataset names are `data1`, `data2`, `data3`, ...
      if (data_idx > 0) {
        snprintf(to_str, sizeof(to_str), "data %lu", data_idx + 1);
      }
      frame_meta_data.set_dataset_name(to_str);
      writer_.Uint(image_data->data.array.data.len);

      this->push(this->offset_index_, boost::allocate_shared<WrapperFrame>(alloc, frame, frame_meta_data,
                          image_data->data.array.data.ptr - reinterpret_cast<uint8_t*>(const_cast<void*>(zmq_message.data()))));
    }
    writer_.EndObject();
    snprintf(to_str, sizeof(to_str), "{\"series_id\": %lu}",  message->series_id); // header
    publish_meta(get_name(), "eiger-image", buffer_.GetString(), to_str);
    LOG4CXX_TRACE(Logger::getLogger("FP.EigerProcessPlugin"), "Finished handling image message");
  }

  /**
   * Handle an end message
   *
   * \param[in] message Parsed stream2 end message
   * \param[in] zmq_message ZMQ message containg raw cbor data blob
   */
  void EigerProcessPlugin::handle_end_msg(struct stream2_end_msg *message, const zmq::message_t &zmq_message)
  {
    // this->notify_end_of_acquisition();  // Need to delay this or we could miss the last few frames
    LOG4CXX_INFO(Logger::getLogger("FP.EigerProcessPlugin"), "Handling end message");
    char to_str[42] = {'\0'};
    snprintf(to_str, sizeof(to_str), "{\"series_id\": %lu}",  message->series_id);
    publish_meta(get_name(), "eiger-end", zmq_message.data(), zmq_message.size(), to_str);
  }

  void EigerProcessPlugin::process_frame(boost::shared_ptr<Frame> frame)
  {
    LOG4CXX_ERROR(Logger::getLogger("FP.EigerProcessPlugin"), "EigerProcessPlugin::process_frame should not be called");
  }

  /**
   * Get the compressed size from a stream2_image_data
   *
   * \param[in] image_data The stream2_image_data to extract compression from
   *
   * \return The compressed size in bytes
   */
  uint32_t EigerProcessPlugin::get_stream2_compressed_size(const struct stream2_image_data *image_data)
  {
    return image_data->data.array.data.len;
  }

  /** Provide configuration readback
   *
   * \param reply[out] - Response IpcMessage to update with current configuration
   */
  void EigerProcessPlugin::requestConfiguration(OdinData::IpcMessage &reply)
  {
    reply.set_param(this->get_name() + '/' + CONFIG_ENDPOINT, this->endpoint_);
    reply.set_param(this->get_name() + '/' + CONFIG_OFFSET_PLUGIN, this->offset_index_);
    reply.set_param(this->get_name() + '/' + CONFIG_RAWFW_PLUGIN, this->rawfilewriter_index_);
  }

  /** Report status
   *
   * \param[out] status - Reference to an IpcMessage value to store the status
   */
  void EigerProcessPlugin::status(OdinData::IpcMessage &status)
  {
    status.set_param(this->get_name() + '/' + STATUS_DROPPED_FRAMES, this->dropped_frames_);
  }

  /** Reset dropped frames counter
   *
   * \returns True if reset was successful
   */
  bool EigerProcessPlugin::reset_statistics()
  {
    this->dropped_frames_ = 0;
    return true;
  }

  int EigerProcessPlugin::get_version_major()
  {
    return EIGER_DETECTOR_VERSION_MAJOR;
  }

  int EigerProcessPlugin::get_version_minor()
  {
    return EIGER_DETECTOR_VERSION_MINOR;
  }

  int EigerProcessPlugin::get_version_patch()
  {
    return EIGER_DETECTOR_VERSION_PATCH;
  }

  std::string EigerProcessPlugin::get_version_short()
  {
    return EIGER_DETECTOR_VERSION_STR_SHORT;
  }

  std::string EigerProcessPlugin::get_version_long()
  {
    return EIGER_DETECTOR_VERSION_STR;
  }

} /* namespace FrameProcessor */
