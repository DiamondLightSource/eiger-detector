#include <zmq/zmq.hpp>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <sys/time.h>
#include <boost/program_options.hpp>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace po = boost::program_options;

int parse_arguments(int argc, char** argv);

void sendHeader(zmq::socket_t& sender, std::string acq_id);
void sendEndOfSeries(zmq::socket_t& sender);
void sendImageData(zmq::socket_t& sender, std::string file_pattern, int frames, int hertz);

void SendFileMessage(zmq::socket_t &socket, std::string filePath, bool more);

std::string acquisition_id = "test";
std::string file_pattern = "streamfile";
std::string port = "9999";
int frames = 1000;
int hertz = 500;
int delay_adjustment = 70000;

void my_free (void *data, void *hint)
{
    // We've allocated the buffer using malloc and
    // at this point we deallocate it using free.
    free (data);
}

/**
 * Simulates an Eiger by sending a stream of data over 0MQ.
 * Reads in frame data from 4 Eiger files that contain the
 * 0MQ stream information, with the file name pattern being
 * specified by a program argument. This one frame is then
 * sent multiple times, incrementing the frame id each time.
 *
 */
int main (int argc, char *argv[]) {

  int rc = parse_arguments(argc, argv);

  if (rc != 0) {
    return rc;
  }

	std::string bindAddress("tcp://*:");
	bindAddress.append(port);

	std::cout << "Binding to " << bindAddress << "..." << std::endl;

	zmq::context_t context(1);

	// Setup socket to send messages on
	zmq::socket_t socket(context, ZMQ_PUSH);
	socket.bind(bindAddress.c_str());

	std::cout << "Socket bound, press enter to send data..." << std::endl;

	getchar();

	sendHeader(socket, acquisition_id);

	sendImageData(socket, file_pattern, frames, hertz);

	sendEndOfSeries(socket);

	std::cout << "Finished Sending messages" << std::endl;

	// Give 0MQ time to deliver
	sleep (1);

	return 0;
}

void sendHeader(zmq::socket_t& sender, std::string acq_id) {
  std::cout << "Sending Header" << std::endl;

  const char* json = "{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"basic\"}";
  std::string jsonpartt2 = "{\"auto_summation\": true, \"photon_energy\": 12658}";

  rapidjson::Document d;
  d.Parse(json);
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);

  std::string part1 = buffer.GetString();

  zmq::message_t message(part1.size());
  memcpy (message.data (), part1.c_str(), part1.size());
  sender.send(message, ZMQ_SNDMORE);

  message.rebuild(jsonpartt2.size());
  memcpy (message.data (), jsonpartt2.c_str(), jsonpartt2.size());
  sender.send(message, ZMQ_SNDMORE);

  std::string appendix = "{\"acqID\":\"" + acq_id + "\"}";
  zmq::message_t appendixMessage(appendix.size());
  memcpy (appendixMessage.data (), appendix.c_str(), appendix.size());
  sender.send(appendixMessage);

  std::cout << "Sent Header" << std::endl;
}

std::string getSingleLineFromFile(std::string file)
{
  std::string line;
  std::ifstream myfile (file.c_str());
  if (myfile.is_open())
  {
    getline (myfile,line);
    myfile.close();
  }
  else
  {
    std::cout << "Unable to open file " << file << std::endl;
    throw;
  }

  return line;
}

void sendImageData(zmq::socket_t& sender, std::string file_pattern, int frames, int hertz) {

  std::cout << "Sending Image Data (" << frames << " frames at " << hertz << " Hertz)" << std::endl;

  zmq::message_t message(10);

  // Read the data from the files to send
  std::string header_file = file_pattern + "_1";
  std::string mdata_file = file_pattern + "_2";
  std::string image_file = file_pattern + "_3";
  std::string times_file = file_pattern + "_4";

  std::streampos size;
  char * memblock;

  bool more = true;

  std::ifstream file (image_file.c_str(), std::ios::in|std::ios::binary|std::ios::ate);
  if (file.is_open())
  {
    size = file.tellg();
    memblock = (char*)malloc(sizeof(char) * size); // no need to free this. free is called by 'my_free' arg to message
    file.seekg (0, std::ios::beg);
    file.read (memblock, size);
    file.close();
  }
  else {
    std::cout << "Unable to open file " << image_file << std::endl;
    return;
  }

  std::cout << "Size of data: " << size << " bytes " << std::endl;

  std::string jsonP1 = getSingleLineFromFile(header_file);
  std::string part2 = getSingleLineFromFile(mdata_file);
  std::string part4 = getSingleLineFromFile(times_file);

  rapidjson::Document documentPart1;
  documentPart1.Parse(jsonP1.c_str());
  rapidjson::Value& series = documentPart1["series"];
  series.SetInt(1);

  struct timespec timeOut,remains;

  double delay = 1 / (double)hertz;

  double nanosec = delay * 1000000000;

  // Adjust the delay based on how long it takes to compute round the loop
  if (nanosec > delay_adjustment)
  {
    nanosec -= delay_adjustment;
  }

  int sec = nanosec / 1000000000;
  nanosec = ((int)nanosec) % 1000000000;

  std::cout << "Using delay of: " << sec << " seconds, " << nanosec << " nanoseconds" << std::endl;

  // Set delay
  timeOut.tv_sec = sec;
  timeOut.tv_nsec = nanosec;

  timeval time_before;
  gettimeofday(&time_before, NULL);

  int task_nbr;
  for (task_nbr = 0; task_nbr < frames; task_nbr++) {

    // Part 1
    rapidjson::StringBuffer buffer1;
    rapidjson::Writer<rapidjson::StringBuffer> writer1(buffer1);
    documentPart1.Accept(writer1);

    rapidjson::Value& frame = documentPart1["frame"];
    frame.SetInt(frame.GetInt() + 1);

    std::string part1 = buffer1.GetString();
    zmq::message_t message(part1.size());
    memcpy (message.data (), part1.c_str(), part1.size());
    sender.send(message, ZMQ_SNDMORE);

    // Part 2
    message.rebuild(part2.size());
    memcpy (message.data (), part2.c_str(), part2.size());
    sender.send(message, ZMQ_SNDMORE);

    // Part 3
    zmq::message_t messageF(memblock, size, NULL);
    sender.send(messageF, ZMQ_SNDMORE);

    // Part 4
    message.rebuild(part4.size());
    memcpy (message.data (), part4.c_str(), part4.size());
    sender.send(message);

    nanosleep(&timeOut, &remains);
  }

  std::cout << "Sent Image Data" << std::endl;

  timeval time_after;
  gettimeofday(&time_after, NULL);

  double timeDiff = (((((time_after.tv_sec - time_before.tv_sec) * 1000000) + (time_after.tv_usec - time_before.tv_usec))/1000.0) / 1000.0);

  std::cout << "Sending took: " << timeDiff << " seconds" << std::endl;

  // Give the messages time to send before freeing the memory by getting input from user
  std::cout << "Press any key to continue..." << std::endl;
  getchar();
  free (memblock);
}

void sendEndOfSeries(zmq::socket_t& sender) {
  std::cout << "Sending End Of Series" << std::endl;

  std::string part1 = "{\"htype\": \"dseries_end-1.0\", \"series\": 1}";

  zmq::message_t message(part1.size());
  memcpy (message.data (), part1.c_str(), part1.size());
  sender.send(message);

  std::cout << "Sent End Of Series" << std::endl;
}

void SendFileMessage(zmq::socket_t &socket, std::string filePath, bool more) {

  std::streampos size;
  char * memblock;

  std::ifstream file (filePath.c_str(), std::ios::in|std::ios::binary|std::ios::ate);
  if (file.is_open())
  {
    size = file.tellg();
    memblock = (char*)malloc(sizeof(char) * size); // no need to free this. free is called by 'my_free' arg to message
    file.seekg (0, std::ios::beg);
    file.read (memblock, size);
    file.close();

    zmq::message_t message(memblock, size, my_free);
    memcpy (message.data (), memblock, size);

    if (more) {
      socket.send(message, ZMQ_SNDMORE);
    } else {
      socket.send(message);
    }
  }
  else {
    std::cout << "Unable to open file " << filePath << std::endl;
  }
}

int parse_arguments(int argc, char** argv)
{
  int rc = 0;
  try
  {
    std::string config_file;

    // Declare a group of options that will allowed only on the command line
    po::options_description generic("Generic options");
    generic.add_options()
        ("help,h",
          "Print this help message")
        ("version,v",
          "Print program version string")
        ("config,c",po::value<std::string>(&config_file),
          "Specify program configuration file")
        ;

    // Declare a group of options that will be allowed both on the command line
    // and in the configuration file
    po::options_description config("Configuration options");
    config.add_options()
        ("acqid,a", po::value<std::string>()->default_value(acquisition_id),
          "Set the acquisition ID")
        ("filepattern,f", po::value<std::string>()->default_value(file_pattern),
          "Set the file pattern to read the data from")
        ("frames,n", po::value<unsigned int>()->default_value(frames),
          "Set the number of frames to send")
        ("hertz,z", po::value<unsigned int>()->default_value(hertz),
          "Set the acquisition speed in hertz")
        ("delay,d", po::value<unsigned int>()->default_value(delay_adjustment),
          "Set the delay adjustment in nanoseconds")
        ("port,p", po::value<std::string>()->default_value(port),
          "Set the port to bind to")
        ;

    // Group the variables for parsing at the command line and/or from the configuration file
    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config);
    po::options_description config_file_options;
    config_file_options.add(config);

    // Parse the command line options
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    // If the command-line help option was given, print help and exit
    if (vm.count("help"))
    {
      std::cout << "usage: eigerSim [options]" << std::endl << std::endl;
      std::cout << "Requires 4 files to exist which contain the 0MQ stream data:" << std::endl;
      std::cout << " - <filepattern>_1 - The dimage-1.0 first message part" << std::endl;
      std::cout << " - <filepattern>_2 - The dimage_d-1.0 second message part" << std::endl;
      std::cout << " - <filepattern>_3 - The raw data" << std::endl;
      std::cout << " - <filepattern>_4 - The dconfig-1.0 fourth message part" << std::endl << std::endl;
      std::cout << cmdline_options << std::endl;
      return 1;
    }

    // If the command line version option was given, print version and exit
    if (vm.count("version"))
    {
      std::cout << "1.0" << std::endl;
      return 1;
    }

    // If the command line config option was given, parse the specified configuration
    // file for additional options. Note that boost::program_options gives precedence
    // to the first instance occurring. In this case, that implies that command-line
    // options have precedence over equivalent configuration file entries.
    if (vm.count("config"))
    {
      std::ifstream config_ifs(config_file.c_str());
      if (config_ifs)
      {
        std::cout << "Parsing configuration file " << config_file;
        po::store(po::parse_config_file(config_ifs, config_file_options, true), vm);
        po::notify(vm);
      }
      else
      {
        std::cout << "Unable to open configuration file " << config_file << " for parsing";
        exit(1);
      }
    }

    if (vm.count("acqid"))
    {
      acquisition_id = vm["acqid"].as<std::string>();
      std::cout << "Using Acquisition ID: " << acquisition_id << std::endl;
    }

    if (vm.count("filepattern"))
    {
      file_pattern = vm["filepattern"].as<std::string>();
      std::cout << "Using File Pattern: " << file_pattern << std::endl;
    }

    if (vm.count("frames"))
    {
      frames = vm["frames"].as<unsigned int>();
      std::cout << "Using number of frames:  " << frames << std::endl;
    }

    if (vm.count("hertz"))
    {
      hertz = vm["hertz"].as<unsigned int>();
      std::cout << "Using hertz: " << hertz << std::endl;
    }

    if (vm.count("delay"))
    {
      delay_adjustment = vm["delay"].as<unsigned int>();
      std::cout << "Using delay adjustment: " << delay_adjustment << std::endl;
    }

    if (vm.count("port"))
    {
      port = vm["port"].as<std::string>();
      std::cout << "Using port: " << port << std::endl;
    }

  }
  catch (...)
  {
    std::cout << "Exception parsing arguments";
    rc = 1;
  }

  return rc;

}
