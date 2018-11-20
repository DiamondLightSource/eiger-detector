#ifndef FRAMESIMULATOR_EIGERFRAMESIMULATORPLUGIN_H
#define FRAMESIMULATOR_EIGERFRAMESIMULATORPLUGIN_H

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

#include <boost/shared_ptr.hpp>
#include <map>
#include <string>
#include <stdexcept>

#include "ClassLoader.h"
#include "FrameSimulatorPlugin.h"

namespace FrameSimulator {

    class EigerFrameSimulatorPlugin : public FrameSimulatorPlugin {

    public:

        EigerFrameSimulatorPlugin();

        virtual void populate_options(po::options_description& config);

        virtual bool setup(const po::variables_map& vm);
        virtual void simulate();

        virtual int get_version_major();
        virtual int get_version_minor();
        virtual int get_version_patch();
        virtual std::string get_version_short();
        virtual std::string get_version_long();


    private:

        /** Pointer to logger **/
        LoggerPtr logger_;

    };

    REGISTER(FrameSimulatorPlugin, EigerFrameSimulatorPlugin, "EigerFrameSimulatorPlugin");

}

#endif 
