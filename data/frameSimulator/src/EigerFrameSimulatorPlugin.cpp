#include <string>

#include "EigerFrameSimulatorPlugin.h"

#include <cstdlib>
#include <time.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include "version.h"

namespace FrameSimulator {

    EigerFrameSimulatorPlugin::EigerFrameSimulatorPlugin() : FrameSimulatorPlugin() {

        // Setup logging for the class
        logger_ = Logger::getLogger("FS.EigerFrameSimulatorPlugin");
        logger_->setLevel(Level::getAll());

    }

  void EigerFrameSimulatorPlugin::populate_options(po::options_description& config) {
    LOG4CXX_DEBUG(logger_, "populating ...");
  }

  bool EigerFrameSimulatorPlugin::setup(const po::variables_map& vm) {
    LOG4CXX_DEBUG(logger_, "setting up ...");
    return true;
  }

  void EigerFrameSimulatorPlugin::simulate() {
    LOG4CXX_DEBUG(logger_, "simulating ...");
  }

    int EigerFrameSimulatorPlugin::get_version_major() {
        return EIGER_DETECTOR_VERSION_MAJOR;
    }

    /**
     * Get the plugin minor version number.
     *
     * \return minor version number as an integer
     */
    int EigerFrameSimulatorPlugin::get_version_minor() {
        return EIGER_DETECTOR_VERSION_MINOR;
    }

    /**
     * Get the plugin patch version number.
     *
     * \return patch version number as an integer
     */
    int EigerFrameSimulatorPlugin::get_version_patch() {
        return EIGER_DETECTOR_VERSION_PATCH;
    }

    /**
     * Get the plugin short version (e.g. x.y.z) string.
     *
     * \return short version as a string
     */
    std::string EigerFrameSimulatorPlugin::get_version_short() {
        return EIGER_DETECTOR_VERSION_STR_SHORT;
    }

    /**
     * Get the plugin long version (e.g. x.y.z-qualifier) string.
     *
     * \return long version as a string
     */
    std::string EigerFrameSimulatorPlugin::get_version_long() {
        return EIGER_DETECTOR_VERSION_STR;
    }


}
