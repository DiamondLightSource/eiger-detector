/*
 * EigerProcessPlugin.h
 *
 *  Created on: 8 May 2017
 *      Author: vtu42223
 */

#ifndef TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_
#define TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

#include "FrameProcessorPlugin.h"
#include "ClassLoader.h"
#include "EigerDefinitions.h"
#include <stdint.h>

namespace FrameProcessor
{
  const int NO_COMPRESSION = 0;
  const int LZ4_COMPRESSION = 1;
  const int BSLZ4_COMPRESSION = 2;
  const int UINT8_DATATYPE = 0;
  const int UINT16_DATATYPE = 1;
  const int UINT32_DATATYPE = 2;

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

  private:
    void process_frame(boost::shared_ptr<Frame> frame);
    void setFrameEncoding(boost::shared_ptr<Frame> frame, const Eiger::FrameHeader* hdrPtr);
    void setFrameDataType(boost::shared_ptr<Frame> frame, const Eiger::FrameHeader* hdrPtr);
    void setFrameDimensions(boost::shared_ptr<Frame> frame, const Eiger::FrameHeader* hdrPtr);

    /** Pointer to logger */
    LoggerPtr logger_;
  };

  /**
   * Registration of this plugin through the ClassLoader.  This macro
   * registers the class without needing to worry about name mangling
   */
  REGISTER(FrameProcessorPlugin, EigerProcessPlugin, "EigerProcessPlugin");

} /* namespace filewriter */

#endif /* TOOLS_FILEWRITER_EIGERPROCESSPLUGIN_H_ */
