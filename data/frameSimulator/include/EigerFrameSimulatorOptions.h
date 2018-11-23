#ifndef FRAMESIMULATOR_EIGERFRAMESIMULATOROPTIONS_H
#define FRAMESIMULATOR_EIGERFRAMESIMULATOROPTIONS_H

#include <string>

#include "FrameSimulatorOption.h"

namespace FrameSimulator {

    static const FrameSimulatorOption<std::string> opt_path("path", "File path");
    static const FrameSimulatorOption<std::string> opt_acqid("acquisition-id", "Acquisition ID", "test");
    static const FrameSimulatorOption<std::string> opt_filepattern("file-pattern", "File pattern", "streamfile");
    static const FrameSimulatorOption<int> opt_delay("delay-adjustment", "Delay adjustment", 70000);

}

#endif //FRAMESIMULATOR_EIGERFRAMESIMULATOROPTIONS_H
