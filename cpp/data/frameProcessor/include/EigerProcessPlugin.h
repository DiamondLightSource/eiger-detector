/*
 * EigerProcessPlugin.h
 *
 *  Created on: 8 May 2017
 *      Authors: Matt Taylor, Gary Yendell
 */

#ifndef TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_
#define TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_

#include <stdint.h>
#include <thread>

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

#include "zmq/zmq.hpp"
#include "stream2.h"

#include "FrameProcessorPlugin.h"
#include "ClassLoader.h"
#include "EigerDefinitions.h"

namespace FrameProcessor
{
  /** Processing of Eiger Frame objects.
   *
   * The EigerProcessPlugin class is responsible for receiving a raw data
   * Frame object and parsing the header information. Depending on the frame type, it
   * sends raw image data on down the chain, or sends meta data out to subscribers.
   */
  class EigerProcessPlugin : public FrameProcessorPlugin
  {
    constexpr static int BODY_CH_SIZE = 2048;
    constexpr static int HEADER_CH_SIZE = 11;
  public:
    EigerProcessPlugin();
    virtual ~EigerProcessPlugin();

    /** Parent class methods */

    int get_version_major();
    int get_version_minor();
    int get_version_patch();
    std::string get_version_short();
    std::string get_version_long();

  private:
    /** Handle data stream socket */
    void handle_rx_socket();
    /** Handle a start message */
    void handle_start_msg(struct stream2_start_msg* message, const zmq::message_t& zmq_message);
    /** Handle a image message */
    void handle_image_msg(struct stream2_image_msg* message, const zmq::message_t& zmq_message);
    /** Handle an end message */
    void handle_end_msg(struct stream2_end_msg* message, const zmq::message_t& zmq_message);

    /** Parse the data type from a stream2 image */
    static inline DataType get_stream2_data_type(const struct stream2_image_data* image_data) 
    {
      uint64_t elem_size = 0;
      stream2_typed_array_elem_size(&image_data->data.array, &elem_size);
      elem_size = ((elem_size > 4) | (elem_size < 1)) ? 0 : elem_size;
      std::array<DataType, 5> type_arr;
      type_arr[0] = DataType::raw_unknown;
      type_arr[1] = DataType::raw_8bit;
      type_arr[2] = DataType::raw_16bit;
      type_arr[3] = DataType::raw_unknown;
      type_arr[4] = DataType::raw_32bit;

      if ((!elem_size) | (elem_size == 3)) {
        LOG4CXX_ERROR(Logger::getLogger("FP.EigerProcessPlugin"), "Unknown frame data type size: " << elem_size);
      }
      return type_arr[elem_size];
    }

    /**
     * Get the compression algorithm from a stream2_image_data
     *
     * \param[in] image_data The stream2_image_data to extract compression from
     */
    static inline std::pair<CompressionType, const char*> get_stream2_compression(const char* compression_str) {
      if (strcmp(compression_str, "bslz4") == 0) {
          return {CompressionType::bslz4, compression_str};
      }
      else if (strcmp(compression_str, "lz4") == 0) {
          return {CompressionType::lz4, compression_str};
      } else {
          return {CompressionType::no_compression, "unknown"};
      }
    }
    /** Parse the total size of the data in a stream2 image */
    uint32_t get_stream2_compressed_size(const struct stream2_image_data* image_data);

    /** Current series id from header to verify image and end messages */
    uint64_t current_series_id_;
    std::string offset_index_;
    std::string rawfilewriter_index_;
    /** Data stream endpoint to connect to */
    std::string endpoint_;
    /** ZeroMQ context */
    zmq::context_t zmq_context_;
    /** ZeroMQ socket for data stream */
    zmq::socket_t zmq_socket_;
    /** Thread for handling data stream socket */
    std::thread rx_thread_;

    /** Frames dropped when failing to allocating memory */
    uint64_t dropped_frames_;
    rapidjson::StringBuffer buffer_;
    rapidjson::StringBuffer header_buffer_;
    rapidjson::Writer<rapidjson::StringBuffer> writer_;
    rapidjson::Writer<rapidjson::StringBuffer> header_writer_;

    static inline const std::string CONFIG_ENDPOINT = "endpoint";
    static inline const std::string CONFIG_OFFSET_PLUGIN = "dropped_frames";
    static inline const std::string CONFIG_RAWFW_PLUGIN = "offset_index";
    static inline const std::string STATUS_DROPPED_FRAMES = "rawfw_index";

    /** Parent class methods */

    void configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply);
    void requestConfiguration(OdinData::IpcMessage& reply);
    void status(OdinData::IpcMessage& status);
    bool reset_statistics();
    /** `process_frame` should not be called as the data is received directly from a socket */
    void process_frame(boost::shared_ptr<Frame> frame);
  };

  /**
   * Registration of this plugin through the ClassLoader.  This macro
   * registers the class without needing to worry about name mangling
   */
  REGISTER(FrameProcessorPlugin, EigerProcessPlugin, "EigerProcessPlugin");

} /* namespace FrameProcessor */

#endif /* TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_ */
