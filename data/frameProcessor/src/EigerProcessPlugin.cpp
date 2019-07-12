/*
 * EigerProcessPlugin.cpp
 *
 *  Created on: 8 May 2017
 *      Author: vtu42223
 */

#include <EigerProcessPlugin.h>

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
    rapidjson::Document document;
    document.SetObject();

    // Add Acquisition ID
    std::string acqIDString(hdrPtr->acquisitionID);
    rapidjson::Value keyAcqID("acqID", document.GetAllocator());
    rapidjson::Value valueAcqID;
    valueAcqID.SetString(acqIDString.c_str(), acqIDString.length(), document.GetAllocator());
    document.AddMember(keyAcqID, valueAcqID, document.GetAllocator());

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
      rapidjson::Value keyFrame("frame", document.GetAllocator());
      rapidjson::Value valueFrame(hdrPtr->frame_number);
      document.AddMember(keyFrame, valueFrame, document.GetAllocator());

      // Add Series number
      rapidjson::Value keySeries("series", document.GetAllocator());
      rapidjson::Value valueSeries(hdrPtr->series);
      document.AddMember(keySeries, valueSeries, document.GetAllocator());

      // Add Size
      rapidjson::Value keySize("size", document.GetAllocator());
      rapidjson::Value valueSize(hdrPtr->size_in_header);
      document.AddMember(keySize, valueSize, document.GetAllocator());

      // Add Start Time
      rapidjson::Value keyStart("start_time", document.GetAllocator());
      rapidjson::Value valueStart(hdrPtr->startTime);
      document.AddMember(keyStart, valueStart, document.GetAllocator());

      // Add Stop Time
      rapidjson::Value keyStop("stop_time", document.GetAllocator());
      rapidjson::Value valueStop(hdrPtr->stopTime);
      document.AddMember(keyStop, valueStop, document.GetAllocator());

      // Add Real Time
      rapidjson::Value keyReal("real_time", document.GetAllocator());
      rapidjson::Value valueReal(hdrPtr->realTime);
      document.AddMember(keyReal, valueReal, document.GetAllocator());

      // Add shape
      rapidjson::Value keyShape("shape", document.GetAllocator());
      rapidjson::Value valueShape;
      valueShape.SetArray();
      valueShape.PushBack(hdrPtr->shapeSizeX, document.GetAllocator());
      valueShape.PushBack(hdrPtr->shapeSizeY, document.GetAllocator());
      document.AddMember(keyShape, valueShape, document.GetAllocator());

      // Add data type
      std::string dataTypeString(hdrPtr->dataType);
      rapidjson::Value keyType("type", document.GetAllocator());
      rapidjson::Value valueType;
      valueType.SetString(dataTypeString.c_str(), dataTypeString.length(), document.GetAllocator());
      document.AddMember(keyType, valueType, document.GetAllocator());

      // Add encoding
      std::string encodingString(hdrPtr->encoding);
      rapidjson::Value keyEncoding("encoding", document.GetAllocator());
      rapidjson::Value valueEncoding;
      valueEncoding.SetString(encodingString.c_str(), encodingString.length(), document.GetAllocator());
      document.AddMember(keyEncoding, valueEncoding, document.GetAllocator());

      // Add hash
      std::string hashString(hdrPtr->hash);
      rapidjson::Value keyHash("hash", document.GetAllocator());
      rapidjson::Value valueHash;
      valueHash.SetString(hashString.c_str(), hashString.length(), document.GetAllocator());
      document.AddMember(keyHash, valueHash, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-imagedata", buffer.GetString(), buffer.GetString());

      this->push(frame);
    } else if (hdrPtr->messageType == Eiger::IMAGE_APPENDIX) {
      std::string dataString((static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size);

      // Add Frame number
      rapidjson::Value keyFrame("frame", document.GetAllocator());
      rapidjson::Value valueFrame(hdrPtr->frame_number);
      document.AddMember(keyFrame, valueFrame, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-imageappendix", dataString, buffer.GetString());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_NONE) {
      // Add Series number
      rapidjson::Value keySeries("series", document.GetAllocator());
      rapidjson::Value valueSeries(hdrPtr->series);
      document.AddMember(keySeries, valueSeries, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-globalnone", buffer.GetString(), buffer.GetString());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_CONFIG) {
      std::string dataString((static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size);

      // Add Series number
      rapidjson::Value keySeries("series", document.GetAllocator());
      rapidjson::Value valueSeries(hdrPtr->series);
      document.AddMember(keySeries, valueSeries, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-globalconfig", dataString, buffer.GetString());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_FLATFIELD) {
      // Add shape
      rapidjson::Value keyShape("shape", document.GetAllocator());
      rapidjson::Value valueShape;
      valueShape.SetArray();
      valueShape.PushBack(hdrPtr->shapeSizeX, document.GetAllocator());
      valueShape.PushBack(hdrPtr->shapeSizeY, document.GetAllocator());
      document.AddMember(keyShape, valueShape, document.GetAllocator());

      // Add data type
      rapidjson::Value keyType("type", document.GetAllocator());
      rapidjson::Value valueType(hdrPtr->dataType);
      document.AddMember(keyType, valueType, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-globalflatfield", reinterpret_cast<const void*>(static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size, buffer.GetString());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_MASK) {
      // Set shape
      rapidjson::Value keyShape("shape", document.GetAllocator());
      rapidjson::Value valueShape;
      valueShape.SetArray();
      valueShape.PushBack(hdrPtr->shapeSizeX, document.GetAllocator());
      valueShape.PushBack(hdrPtr->shapeSizeY, document.GetAllocator());
      document.AddMember(keyShape, valueShape, document.GetAllocator());

      // Set data type
      rapidjson::Value keyType("type", document.GetAllocator());
      rapidjson::Value valueType(hdrPtr->dataType);
      document.AddMember(keyType, valueType, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-globalmask", reinterpret_cast<const void*>(static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size, buffer.GetString());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_COUNTRATE) {
      // Set shape
      rapidjson::Value keyShape("shape", document.GetAllocator());
      rapidjson::Value valueShape;
      valueShape.SetArray();
      valueShape.PushBack(hdrPtr->shapeSizeX, document.GetAllocator());
      valueShape.PushBack(hdrPtr->shapeSizeY, document.GetAllocator());
      document.AddMember(keyShape, valueShape, document.GetAllocator());

      // Set data type
      rapidjson::Value keyType("type", document.GetAllocator());
      rapidjson::Value valueType(hdrPtr->dataType);
      document.AddMember(keyType, valueType, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-globalcountrate", reinterpret_cast<const void*>(static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size, buffer.GetString());
    } else if (hdrPtr->messageType == Eiger::GLOBAL_HEADER_APPENDIX) {
      std::string dataString((static_cast<const char*>(frame->get_image_ptr())+sizeof(Eiger::FrameHeader)), hdrPtr->data_size);

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      document.Accept(writer);

      publish_meta(get_name(), "eiger-headerappendix", dataString, buffer.GetString());
    } else if (hdrPtr->messageType == Eiger::END_OF_STREAM) {
      // Do nothing with End of Stream message
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
