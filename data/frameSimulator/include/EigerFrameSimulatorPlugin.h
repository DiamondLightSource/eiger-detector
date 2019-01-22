#ifndef FRAMESIMULATOR_EIGERFRAMESIMULATORPLUGIN_H
#define FRAMESIMULATOR_EIGERFRAMESIMULATORPLUGIN_H

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <map>
#include <string>
#include <stdexcept>

#include "ClassLoader.h"
#include "FrameSimulatorPlugin.h"

#include <zmq/zmq.hpp>

namespace FrameSimulator {

    /** EigerFrameSimulatorPlugin
     * Simulates an Eiger by sending a stream of data over 0MQ.
     * Reads in frame data from 4 Eiger files that contain the
     * 0MQ stream information, with the file name pattern being
            * specified by a program argument. This one frame is then
            * sent multiple times, incrementing the frame id each time.
     *
     */
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

        void sendHeader(zmq::socket_t& sender, std::string acq_id);
        void sendEndOfSeries(zmq::socket_t& sender);
        void sendImageData(zmq::socket_t& sender, std::string file_pattern, int frames, int hertz);
        void SendFileMessage(zmq::socket_t &socket, std::string filePath, bool more);
        std::string getSingleLineFromFile(std::string file);

        boost::optional<std::string> filepath;
        std::string filepattern;
        std::string acquisitionID;
        int delay_adjustment;
        std::vector<std::string> dest_ports;

    };

    REGISTER(FrameSimulatorPlugin, EigerFrameSimulatorPlugin, "EigerFrameSimulatorPlugin");

}

#endif 
