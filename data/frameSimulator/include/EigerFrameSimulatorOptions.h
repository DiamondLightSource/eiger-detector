#ifndef FRAMESIMULATOR_EIGERFRAMESIMULATOROPTIONS_H
#define FRAMESIMULATOR_EIGERFRAMESIMULATOROPTIONS_H

#include <string>

#include "FrameSimulatorOption.h"

namespace FrameSimulator {

    /** EigerFrameSimulatorPlugin simulator options
     * specific options for an eiger frame simulator
     */
    static const FrameSimulatorOption<std::string> opt_path("path", "File path");
    static const FrameSimulatorOption<std::string> opt_acqid("acquisition-id", "Acquisition ID", "test");
    static const FrameSimulatorOption<std::string> opt_filepattern("file-pattern", "File pattern", "streamfile");
    static const FrameSimulatorOption<int> opt_delay("delay-adjustment", "Delay adjustment", 70000);
    static const FrameSimulatorOption<bool> opt_stream("stream", "Stream mode", false);
    static const FrameSimulatorOption<bool> opt_user_prompt("prompt", "User prompt delay enable", false);
}

#endif //FRAMESIMULATOR_EIGERFRAMESIMULATOROPTIONS_H
