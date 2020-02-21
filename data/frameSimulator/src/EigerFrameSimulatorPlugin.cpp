#include <string>

#include "EigerFrameSimulatorPlugin.h"
#include "EigerFrameSimulatorOptions.h"
#include "FrameSimulatorOption.h"

#include <cstdlib>
#include <time.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <unistd.h>

#include "version.h"

#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <sys/time.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace FrameSimulator {

    /** helper function to free memory in buffer
     * We've allocated the buffer using malloc and
     * At this point we deallocate it using free.
     * @param data
     * @param hint
     */
    void my_free(void *data, void *hint) {
      free(data);
    }

    /** Construct an ExcaliburFrameSimulatorPlugin
     * setup an instance of the logger
     */
    EigerFrameSimulatorPlugin::EigerFrameSimulatorPlugin() : FrameSimulatorPlugin() {

      // Setup logging for the class
      logger_ = Logger::getLogger("FS.EigerFrameSimulatorPlugin");
      logger_->setLevel(Level::getAll());

    }

    /** Populate boost program options with appropriate command line options for plugin
     * /param[out] config - boost::program_options::options_description to populate with
     * appropriate plugin command line options
     */
    void EigerFrameSimulatorPlugin::populate_options(po::options_description &config) {

      FrameSimulatorPlugin::populate_options(config);

      opt_path.add_option_to(config);
      opt_acqid.add_option_to(config);
      opt_filepattern.add_option_to(config);
      opt_delay.add_option_to(config);
      opt_stream.add_option_to(config);

    }

    /** Setup eiger frame simulator plugin class from store of command line options
     *
     * \param[in] vm - store of given command line options
     * \return true (false) if required program options are (not) specified
     * and the simulator plugin is not ready to use
     *
     * Derived plugins must additionally call the base method even if overridden
     */
    bool EigerFrameSimulatorPlugin::setup(const po::variables_map &vm) {

      if (!FrameSimulatorPlugin::setup(vm))
        return false;

      if (!opt_acqid.is_specified(vm)) {
        LOG4CXX_ERROR(logger_, "Acquisition ID not specified");
        return false;
      }

      if (!opt_filepattern.is_specified(vm)) {
        LOG4CXX_ERROR(logger_, "File pattern not specified");
        return false;
      }

      if (!opt_frames.is_specified(vm)) {
        LOG4CXX_ERROR(logger_, "Number frames not specified");
        return false;
      }

      if (!opt_interval.is_specified(vm)) {
        LOG4CXX_ERROR(logger_, "Interval not specified");
        return false;
      }

      if (!opt_delay.is_specified(vm)) {
        LOG4CXX_ERROR(logger_, "Delay adjustment not specified");
        return false;
      }

      stream = opt_stream.get_val(vm);
      filepattern = opt_filepattern.get_val(vm);
      acquisitionID = opt_acqid.get_val(vm);
      delay_adjustment = opt_delay.get_val(vm);

      set_list_option(opt_ports.get_val(vm), dest_ports);

      if (dest_ports.size() > 1)
        LOG4CXX_WARN(logger_, "Single port support only; using first port.");

      if (opt_path.is_specified(vm)) {
        filepath = opt_path.get_val(vm);
        LOG4CXX_INFO(logger_, "Setting file path");
      }

      return true;

    }

    /** Simulate detector by replaying frames
     *
     */
    void EigerFrameSimulatorPlugin::simulate() {

      std::string bindAddress("tcp://*:");
      bindAddress.append(dest_ports[0]);

      LOG4CXX_INFO(logger_, "Binding to " + bindAddress + " ...");

      zmq::context_t context(1);

      // Setup socket to send messages on
      zmq::socket_t socket(context, ZMQ_PUSH);
      socket.bind(bindAddress.c_str());

      LOG4CXX_INFO(logger_, "Sending messages...");

      if(!stream) {

        std::cout << "Socket bound, press enter to send header..." << std::endl;
        getchar();
        sendHeader(socket, acquisitionID);

        std::cout << "Press enter to send data..." << std::endl;
        getchar();
        double hertz = 1 / replay_interval.get();
        sendImageData(socket, filepattern, replay_numframes.get(), hertz);

        sendEndOfSeries(socket);

      }

      else {

        // Example use for system test data. Replace with the path to actual files.
        SendFileMessage(socket, "streamfile_1", true);  // Global Header
        SendFileMessage(socket, "streamfile_2", true);  // Global Header Config.
        SendFileMessage(socket, "streamfile_3", true);  // Flatfield Header
        SendFileMessage(socket, "streamfile_4", true);  // Flatfield data blob
        SendFileMessage(socket, "streamfile_5", true);  // Pixelmask header
        SendFileMessage(socket, "streamfile_6", true);  // Pixelmask data blob
        SendFileMessage(socket, "streamfile_7", true);  // Countrate header
        SendFileMessage(socket, "streamfile_8", true);  // Countrate data blob
        SendFileMessage(socket, "streamfile_9",
                        false); // Global Header Appendix. 'more' set to false as is the last message in a multipart message

        SendFileMessage(socket, "streamfile_10", true);  // Image data header
        SendFileMessage(socket, "streamfile_11", true);  // Image data dimensions
        SendFileMessage(socket, "streamfile_12", true);  // Image data blob
        SendFileMessage(socket, "streamfile_13", false); // Image data times.

        SendFileMessage(socket, "streamfile_14", true);  // Image data header
        SendFileMessage(socket, "streamfile_15", true);  // Image data dimensions
        SendFileMessage(socket, "streamfile_16", true);  // Image data blob
        SendFileMessage(socket, "streamfile_17", false); // Image data times.

        SendFileMessage(socket, "streamfile_18", true);  // Image data header
        SendFileMessage(socket, "streamfile_19", true);  // Image data dimensions
        SendFileMessage(socket, "streamfile_20", true);  // Image data blob
        SendFileMessage(socket, "streamfile_21", false); // Image data times.

        SendFileMessage(socket, "streamfile_22", false); // End message

      }

      LOG4CXX_INFO(logger_, "Finished Sending messages");

      sleep(1);              //  Give 0MQ time to deliver

    }

    void EigerFrameSimulatorPlugin::SendFileMessage(zmq::socket_t &socket, std::string filePath, bool more) {

      std::streampos size;
      char *memblock;

      std::string full_filepath = filePath;
      if (filepath)
        full_filepath = filepath.get() + filePath;
      std::ifstream file(full_filepath.c_str(),
                         std::ios::in | std::ios::binary | std::ios::ate);
      if (file.is_open()) {
        size = file.tellg();
        memblock = (char *) malloc(
                sizeof(char) * size); // no need to free this. free is called by 'my_free' arg to message
        file.seekg(0, std::ios::beg);
        file.read(memblock, size);
        file.close();

        zmq::message_t message(memblock, size, my_free);
        memcpy(message.data(), memblock, size);

        if (more) {
          socket.send(message, ZMQ_SNDMORE);
        } else {
          socket.send(message);
        }
      } else {
        LOG4CXX_ERROR(logger_, "Unable to open file " << filePath);
      }
    }

    /** Send the header
     * @param sender - 0MQ socket
     * @param acq_id - acquisition id
     */
    void EigerFrameSimulatorPlugin::sendHeader(zmq::socket_t &sender, std::string acq_id) {

      LOG4CXX_INFO(logger_, "Sending Header");

      const char *json = "{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"basic\"}";
      std::string jsonpartt2 = "{\"auto_summation\": true, \"photon_energy\": 12658}";

      rapidjson::Document d;
      d.Parse(json);
      rapidjson::StringBuffer buffer;
      rapidjson::Writer <rapidjson::StringBuffer> writer(buffer);
      d.Accept(writer);

      std::string part1 = buffer.GetString();

      zmq::message_t message(part1.size());
      memcpy(message.data(), part1.c_str(), part1.size());
      sender.send(message, ZMQ_SNDMORE);

      message.rebuild(jsonpartt2.size());
      memcpy(message.data(), jsonpartt2.c_str(), jsonpartt2.size());
      sender.send(message, ZMQ_SNDMORE);

      std::string appendix = "{\"acqID\":\"" + acq_id + "\"}";
      zmq::message_t appendixMessage(appendix.size());
      memcpy(appendixMessage.data(), appendix.c_str(), appendix.size());
      sender.send(appendixMessage);

      LOG4CXX_INFO(logger_, "Sent Header");

    }

    /** Read single line from a file
     * @param file - file name and path
     * @return line as std::string
     */
    std::string EigerFrameSimulatorPlugin::getSingleLineFromFile(std::string file) {
      std::string line;
      std::ifstream myfile(file.c_str());
      if (myfile.is_open()) {
        getline(myfile, line);
        myfile.close();
      } else {
        LOG4CXX_ERROR(logger_, "Unable to open file " + file);
        throw;
      }

      return line;
    }

    /** send the image data
     *
     * @param sender - 0MQ socket
     * @param file_pattern - data in files <file_pattern>_1, <file_pattern>_2, <file_pattern>_3, <file_pattern>_4
     * @param frames - the number of times to send the image data
     * @param hertz - frequency at which the frames are sent
     */
    void
    EigerFrameSimulatorPlugin::sendImageData(zmq::socket_t &sender, std::string file_pattern, int frames, int hertz) {

      LOG4CXX_INFO(logger_, "Sending Image Data (" + boost::lexical_cast<std::string>(frames) + " frames at " +
                            boost::lexical_cast<std::string>(hertz) + " Hertz)");

      zmq::message_t message(10);

      // Read the data from the files to send
      std::string header_file = file_pattern + "_1";
      std::string mdata_file = file_pattern + "_2";
      std::string image_file = file_pattern + "_3";
      std::string times_file = file_pattern + "_4";

      if (filepath) {
        header_file.insert(0, filepath.get() + "/");
        mdata_file.insert(0, filepath.get() + "/");
        image_file.insert(0, filepath.get() + "/");
        times_file.insert(0, filepath.get() + "/");
      }

      std::streampos size;
      char *memblock;

      bool more = true;

      std::ifstream file(image_file.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
      if (file.is_open()) {
        size = file.tellg();
        memblock = (char *) malloc(
                sizeof(char) * size); // no need to free this. free is called by 'my_free' arg to message
        file.seekg(0, std::ios::beg);
        file.read(memblock, size);
        file.close();
      } else {
        LOG4CXX_ERROR(logger_, "Unable to open file " + image_file);
        return;
      }

      LOG4CXX_INFO(logger_, "Size of data: " + boost::lexical_cast<std::string>(size) + " bytes");

      std::string jsonP1 = getSingleLineFromFile(header_file);
      std::string part2 = getSingleLineFromFile(mdata_file);
      std::string part4 = getSingleLineFromFile(times_file);

      rapidjson::Document documentPart1;
      documentPart1.Parse(jsonP1.c_str());
      rapidjson::Value &series = documentPart1["series"];
      series.SetInt(1);

      struct timespec timeOut, remains;

      double delay = 1 / (double) hertz;

      double nanosec = delay * 1000000000;

      // Adjust the delay based on how long it takes to compute round the loop
      if (nanosec > delay_adjustment) {
        nanosec -= delay_adjustment;
      }

      int sec = nanosec / 1000000000;
      nanosec = ((int) nanosec) % 1000000000;

      LOG4CXX_INFO(logger_, "Using delay of: " + boost::lexical_cast<std::string>(sec) + " seconds, " +
                            boost::lexical_cast<std::string>(nanosec) + " nanoseconds");

      // Set delay
      timeOut.tv_sec = sec;
      timeOut.tv_nsec = nanosec;

      timeval time_before;
      gettimeofday(&time_before, NULL);

      int task_nbr;
      for (task_nbr = 0; task_nbr < frames; task_nbr++) {

        // Part 1
        rapidjson::StringBuffer buffer1;
        rapidjson::Writer <rapidjson::StringBuffer> writer1(buffer1);
        documentPart1.Accept(writer1);

        rapidjson::Value &frame = documentPart1["frame"];
        frame.SetInt(frame.GetInt() + 1);

        std::string part1 = buffer1.GetString();
        zmq::message_t message(part1.size());
        memcpy(message.data(), part1.c_str(), part1.size());
        sender.send(message, ZMQ_SNDMORE);

        // Part 2
        message.rebuild(part2.size());
        memcpy(message.data(), part2.c_str(), part2.size());
        sender.send(message, ZMQ_SNDMORE);

        // Part 3
        zmq::message_t messageF(memblock, size, NULL);
        sender.send(messageF, ZMQ_SNDMORE);

        // Part 4
        message.rebuild(part4.size());
        memcpy(message.data(), part4.c_str(), part4.size());
        sender.send(message);

        nanosleep(&timeOut, &remains);
      }

      LOG4CXX_INFO(logger_, "Sent Image Data");

      timeval time_after;
      gettimeofday(&time_after, NULL);

      double timeDiff = (
              ((((time_after.tv_sec - time_before.tv_sec) * 1000000) + (time_after.tv_usec - time_before.tv_usec)) /
               1000.0) / 1000.0);

      LOG4CXX_INFO(logger_, "Sending took: " + boost::lexical_cast<std::string>(timeDiff) + " seconds");

      // Give the messages time to send before freeing the memory by getting input from user
      std::cout << "Press any key to continue ..." << std::endl;
      getchar();

      free(memblock);
    }

    /** send of of series information
     * @param sender - 0MQ socket
     */
    void EigerFrameSimulatorPlugin::sendEndOfSeries(zmq::socket_t &sender) {

      LOG4CXX_DEBUG(logger_, "Sending End Of Series");

      std::string part1 = "{\"htype\": \"dseries_end-1.0\", \"series\": 1}";

      zmq::message_t message(part1.size());
      memcpy(message.data(), part1.c_str(), part1.size());
      sender.send(message);

      LOG4CXX_DEBUG(logger_, "Sent End Of Series");
    }

    /**
     * Get the plugin major version number.
     *
     * \return minor version number as an integer
     */
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
