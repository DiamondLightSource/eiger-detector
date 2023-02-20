/*
 * EigerProcessPlugin.h
 *
 *  Created on: 8 May 2017
 *      Authors: Matt Taylor, Gary Yendell
 */

#ifndef TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_
#define TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_

#include <stdint.h>

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
  public:
    EigerProcessPlugin();
    virtual ~EigerProcessPlugin();

    int get_version_major();
    int get_version_minor();
    int get_version_patch();
    std::string get_version_short();
    std::string get_version_long();

  private:
    void configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply);
    void requestConfiguration(OdinData::IpcMessage& reply);

    void handle_rx_socket();
    void handle_start_msg(struct stream2_start_msg* message, const zmq::message_t& zmq_message);
    void handle_image_msg(struct stream2_image_msg* message, boost::shared_ptr<Frame> frame);
    void handle_end_msg(struct stream2_end_msg* message, const zmq::message_t& zmq_message);
    void process_frame(boost::shared_ptr<Frame> frame);

    DataType get_stream2_data_type(const struct stream2_image_data* image_data);
    dimensions_t get_stream2_dimensions(const struct stream2_image_data* image_data);
    CompressionType get_stream2_compression(const struct stream2_image_data* image_data);
    uint32_t get_stream2_compressed_size(const struct stream2_image_data* image_data);

    uint64_t current_series_id_;
    std::string endpoint_;
    zmq::context_t zmq_context_;
    zmq::socket_t zmq_socket_;
    boost::shared_ptr<boost::thread> rx_thread_;
    /** Mutex used to make this class thread safe */
    boost::recursive_mutex mutex_;
    bool shutdown_;

    /** Pointer to logger */
    LoggerPtr logger_;

    static const std::string CONFIG_ENDPOINT;

  };

  /**
   * Registration of this plugin through the ClassLoader.  This macro
   * registers the class without needing to worry about name mangling
   */
  REGISTER(FrameProcessorPlugin, EigerProcessPlugin, "EigerProcessPlugin");

} /* namespace FrameProcessor */

#endif /* TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_ */
