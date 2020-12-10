/*
 * EigerProcessPlugin.cpp
 *
 *  Created on: 8 May 2017
 *      Author: Matt Taylor
 */

#include <EigerProcessPlugin.h>
#include "Json.h"

namespace FrameProcessor
{

  /**
   * Constuctor
   */
  EigerProcessPlugin::EigerProcessPlugin()
  {
    // Setup logging for the class
    logger_ = Logger::getLogger("FP.EigerProcessPlugin");
    logger_->setLevel(Level::getAll());
    LOG4CXX_TRACE(logger_, "EigerProcessPlugin constructor.");
  }

  /**
   * Destructor
   */
  EigerProcessPlugin::~EigerProcessPlugin()
  {
  }

  /**
   * Processes a frame
   *
   * \param[in] frame The frame to process
   */
  void EigerProcessPlugin::process_frame(boost::shared_ptr<Frame> frame)
  {
    const Eiger::FrameHeader* hdrPtr = static_cast<const Eiger::FrameHeader*>(frame->get_image_ptr());

    LOG4CXX_TRACE(logger_, "FrameHeader frame currentMessageType: " << hdrPtr->messageType);
    LOG4CXX_TRACE(logger_, "FrameHeader frame series: " << hdrPtr->series);
    LOG4CXX_TRACE(logger_, "FrameHeader frame number: " << hdrPtr->frame_number);
    LOG4CXX_TRACE(logger_, "FrameHeader frame shapeSizeX: " << hdrPtr->shapeSizeX);
    LOG4CXX_TRACE(logger_, "FrameHeader frame shapeSizeY: " << hdrPtr->shapeSizeY);
    LOG4CXX_TRACE(logger_, "FrameHeader frame shapeSizeZ: " << hdrPtr->shapeSizeZ);
    LOG4CXX_TRACE(logger_, "FrameHeader frame startTime: " << hdrPtr->startTime);
    LOG4CXX_TRACE(logger_, "FrameHeader frame stopTime: " << hdrPtr->stopTime);
    LOG4CXX_TRACE(logger_, "FrameHeader frame realTime: " << hdrPtr->realTime);
    LOG4CXX_TRACE(logger_, "FrameHeader frame blob_size: " << hdrPtr->data_size);
    LOG4CXX_TRACE(logger_, "FrameHeader frame data type: " << hdrPtr->dataType);
    LOG4CXX_TRACE(logger_, "FrameHeader frame encoding: " << hdrPtr->encoding);
    LOG4CXX_TRACE(logger_, "FrameHeader frame acquisition ID: " << hdrPtr->acquisitionID);

    // Create status message header

    // Add Acquisition ID
    std::string acqIDString(hdrPtr->acquisitionID);
    OdinData::JsonDict json;
    json.add("acqID", acqIDString);

    if (hdrPtr->messageType == Eiger::IMAGE_DATA) {
      frame->set_image_offset(sizeof(Eiger::FrameHeader));
      frame->set_image_size(hdrPtr->data_size);

      FrameMetaData frame_meta_data;

      frame_meta_data.set_dataset_name("data");

      setFrameEncoding(frame_meta_data, hdrPtr);
      setFrameDataType(frame_meta_data, hdrPtr);
      setFrameDimensions(frame_meta_data, hdrPtr);
      frame_meta_data.set_acquisition_ID(hdrPtr->acquisitionID);

      // Set the compressed_size parameter to the frame size
      frame_meta_data.set_parameter("compressed_size", hdrPtr->data_size);

      frame->set_meta_data(frame_meta_data);
      frame->set_frame_number(hdrPtr->frame_number);

      // Add Frame number
      json.add("frame", hdrPtr->frame_number);

      // Add Series number
      json.add("series", hdrPtr->series);

      // Add Size
      json.add("size", hdrPtr->size_in_header);

      // Add Start Time
      json.add("start_time", hdrPtr->startTime);

      // Add Stop Time
      json.add("stop_time", hdrPtr->stopTime);

      // Add Real Time
      json.add("real_time", hdrPtr->realTime);

      // Add shape
      std::vector<uint32_t> shape;
      shape.push_back(hdrPtr->shapeSizeX);
      shape.push_back(hdrPtr->shapeSizeY);
      json.add("shape", shape);

      // Add data type
      std::string dataTypeString(hdrPtr->dataType);
      json.add("type", dataTypeString);

      // Add encoding
      std::string encodingString(hdrPtr->encoding);
      json.add("encoding", encodingString);

      // Add hash
      std::string hashString(hdrPtr->hash);
      json.add("hash", hashString);

      publish_meta(get_name(), "eiger-imagedata", json.str(), json.str());

      this->push(frame);
    } else if (hdrPtr->messageType == Eiger::IMAGE_APPENDIX) {
      std::string dataString((static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size);

      // Add Frame number
      json.add("frame", hdrPtr->frame_number);

      publish_meta(get_name(), "eiger-imageappendix", dataString, json.str());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_NONE) {
      // Add Series number
      json.add("series", hdrPtr->series);

      publish_meta(get_name(), "eiger-globalnone", json.str(), json.str());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_CONFIG) {
      std::string dataString((static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size);

      // Add Series number
      json.add("series", hdrPtr->series);

      publish_meta(get_name(), "eiger-globalconfig", dataString, json.str());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_FLATFIELD) {
      // Add shape
      std::vector<uint32_t> shape;
      shape.push_back(hdrPtr->shapeSizeX);
      shape.push_back(hdrPtr->shapeSizeY);
      json.add("shape", shape);

      // Add data type
      std::string dataTypeString(hdrPtr->dataType);
      json.add("type", dataTypeString);

      publish_meta(get_name(), "eiger-globalflatfield", reinterpret_cast<const void*>(static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size, json.str());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_MASK) {
      // Add shape
      std::vector<uint32_t> shape;
      shape.push_back(hdrPtr->shapeSizeX);
      shape.push_back(hdrPtr->shapeSizeY);
      json.add("shape", shape);

      // Add data type
      std::string dataTypeString(hdrPtr->dataType);
      json.add("type", dataTypeString);

      publish_meta(get_name(), "eiger-globalmask", reinterpret_cast<const void*>(static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size, json.str());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_COUNTRATE) {
      // Add shape
      std::vector<uint32_t> shape;
      shape.push_back(hdrPtr->shapeSizeX);
      shape.push_back(hdrPtr->shapeSizeY);
      json.add("shape", shape);

      // Add data type
      std::string dataTypeString(hdrPtr->dataType);
      json.add("type", dataTypeString);

      publish_meta(get_name(), "eiger-globalcountrate", reinterpret_cast<const void*>(static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size, json.str());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_APPENDIX) {
      std::string dataString((static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size);

      publish_meta(get_name(), "eiger-headerappendix", dataString, json.str());
    } else if (hdrPtr->messageType == Eiger::END_OF_STREAM) {
      // Add Series number
      json.add("series", hdrPtr->series);

      publish_meta(get_name(), "eiger-end", "", json.str());
    }
  }

  /**
   * Set the encoding on the frame
   *
   * \param[out] frame The frame Meta Data object to set the encoding on
   * \param[in] hdrPtr The header containing the encoding
   */
  void EigerProcessPlugin::setFrameEncoding(FrameMetaData &frame, const Eiger::FrameHeader* hdrPtr) {
    std::string encoding(hdrPtr->encoding);

    // Parse out lz4
    std::size_t found = encoding.find("lz4");
    if (found != std::string::npos) {
      found = encoding.find("bs");
      if (found != std::string::npos) {
        frame.set_compression_type(bslz4);
      } else {
        frame.set_compression_type(lz4);
      }
    } else {
      frame.set_compression_type(no_compression);
    }
  }

  /**
   * Set the data type on the frame
   *
   * \param[out] frame The frame Meta Data object to set the encoding on
   * \param[in] hdrPtr The header containing the encoding
   */
  void EigerProcessPlugin::setFrameDataType(FrameMetaData &frame, const Eiger::FrameHeader* hdrPtr) {
    std::string dataType(hdrPtr->dataType);

    if (dataType.compare("uint8") == 0) {
      frame.set_data_type(raw_8bit);
    } else if (dataType.compare("uint16") == 0) {
      frame.set_data_type(raw_16bit);
    } else if (dataType.compare("uint32") == 0) {
      frame.set_data_type(raw_32bit);
    } else {
      LOG4CXX_ERROR(logger_, "Unknown frame data type :" << dataType);
    }
  }

  /**
   * Set the dimensions on the frame
   *
   * \param[out] frame The frame Meta Data object to set the encoding on
   * \param[in] hdrPtr The header containing the dimensions
   */
  void EigerProcessPlugin::setFrameDimensions(FrameMetaData &frame, const Eiger::FrameHeader* hdrPtr) {
    dimensions_t dims;
    if (hdrPtr->shapeSizeZ > 0) {
      dims.push_back(hdrPtr->shapeSizeZ);
    }
    dims.push_back(hdrPtr->shapeSizeY);
    dims.push_back(hdrPtr->shapeSizeX);

    frame.set_dimensions(dims);
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
