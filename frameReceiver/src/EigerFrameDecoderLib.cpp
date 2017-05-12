/*
 * EIGERFrameDecoderLib.cpp
 *
 *  Created on: 8 Mar 2017
 *      Author: gnx91527
 */


#include "EigerFrameDecoder.h"
#include "ClassLoader.h"

namespace FrameReceiver
{
    /**
     * Registration of this decoder through the ClassLoader.  This macro
     * registers the class without needing to worry about name mangling
     */
    REGISTER(FrameDecoder, EigerFrameDecoder, "EigerFrameDecoder");

} // namespace FrameReceiver



