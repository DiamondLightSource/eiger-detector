/*!
 * eigerfan_main.cpp
 *
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <locale.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>
#include <pthread.h> 

#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
#include <log4cxx/xml/domconfigurator.h>
#include <log4cxx/mdc.h>
#include <boost/program_options.hpp>
#include "EigerFan.h"
#include "EigerFanConfig.h"

using namespace log4cxx;
using namespace log4cxx::helpers;

namespace po = boost::program_options;

static bool has_suffix(const std::string &str, const std::string &suffix)
{
  return str.size() >= suffix.size() &&
      str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int parse_arguments(int argc, char** argv, EigerFanConfig &cfg);

/** Configure logging constants
 *
 * Call this only once per thread context.
 *
 * @param app_path Path to executable (use argv[0])
 */
void configure_logging_mdc(const char* app_path)
{
    char hostname[HOST_NAME_MAX];
    ::gethostname(hostname, HOST_NAME_MAX); hostname[HOST_NAME_MAX-1]='\0';
    MDC::put("host", hostname);

    std::stringstream ss;
    ss << ::getpid();
    MDC::put("pid", ss.str());

    const char * app_name = strrchr(app_path, static_cast<int>('/'));
    if (app_name != NULL) {
    	MDC::put("app", app_name+1);
    } else {
    	MDC::put("app", app_path);
    }

    char thread_name[256];
    thread_name[0]='\0';
    thread_name[256-1]='\0';
    ::pthread_getname_np(::pthread_self(), thread_name, 256);
    MDC::put("thread", thread_name);

    uid_t uid = ::geteuid();
    struct passwd *pw = ::getpwuid(uid);
    if (pw) {
      MDC::put("user", pw->pw_name);
    }
}


int main(int argc, char *argv[])
{
    setlocale(LC_CTYPE, "UTF-8");

    configure_logging_mdc(argv[0]);
    LoggerPtr log(Logger::getLogger("ED.APP"));
    LOG4CXX_INFO(log, "Starting EigerFan application...");

    EigerFanConfig cfg;

    int rc = parse_arguments(argc, argv, cfg);

    if (rc == 0) {
		EigerFan eiger_fan(cfg);

		eiger_fan.run();
    }

    return rc;
}

//! Parse command-line arguments and configuration file options.
//!
//! This method parses command-line arguments and configuration file options
//! to configure the application for operation. Most options can either be
//! given at the command line or stored in an INI-formatted configuration file.
//! The configuration options are stored in a EigerFanConfig helper object for
//! retrieval throughout the application.
//!
//! \param argc - standard command-line argument count
//! \param argv - array of char command-line options
//! \param cfg  - the EigerFanConfig object to populate with the options
//! \return return code, 0 if OK, 1 if option parsing failed

int parse_arguments(int argc, char** argv, EigerFanConfig &cfg)
{
	LoggerPtr logger(Logger::getLogger("ED.APP"));
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
        ("logconfig,l", po::value<std::string>(),
           "Set the log4cxx logging configuration file")
				("threads,t", po::value<unsigned int>()->default_value(EigerFanDefaults::DEFAULT_NUM_THREADS),
					"Set the number of 0MQ threads for the 0MQ context")
				("consumers,n", po::value<unsigned int>()->default_value(EigerFanDefaults::DEFAULT_NUM_CONSUMERS),
					"Set the number of expected consumers")
				("fanport,f", po::value<unsigned int>()->default_value(EigerFanDefaults::DEFAULT_FAN_PORT_NUMBER_START),
					"Set the lowest port to accept consumer connections on. Further connections are made on subsequent sequential ports")
				("ctrlport,p", po::value<std::string>()->default_value(EigerFanDefaults::DEFAULT_CONTROL_PORT_NUMBER),
					"Set the port to accept control messages on")
				("addr,s", po::value<std::string>()->default_value(EigerFanDefaults::DEFAULT_STREAM_ADDRESS),
					"Set the address of the stream to connect to")
				("sockets,z", po::value<unsigned int>()->default_value(EigerFanDefaults::DEFAULT_NUM_SOCKETS),
					"Set the number of zmq sockets to connect to the Eiger with")
				("blocksize,b", po::value<unsigned int>()->default_value(EigerFanDefaults::DEFAULT_BLOCK_SIZE),
					"Set the block size being used by the downstream data file writers to")
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
		    std::cout << "usage: frameReceiver [options]" << std::endl << std::endl;
			std::cout << cmdline_options << std::endl;
			return 1;
		}

		// If the command line version option was given, print version and exit
		if (vm.count("version"))
		{
			std::cout << "Will print version here" << std::endl;
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
				LOG4CXX_DEBUG(logger, "Parsing configuration file " << config_file);
				po::store(po::parse_config_file(config_ifs, config_file_options, true), vm);
				po::notify(vm);
			}
			else
			{
				LOG4CXX_ERROR(logger, "Unable to open configuration file " << config_file << " for parsing");
				exit(1);
			}
		}

    if (vm.count("logconfig"))
    {
      std::string logconf_fname = vm["logconfig"].as<std::string>();
      if (has_suffix(logconf_fname, ".xml")) {
        log4cxx::xml::DOMConfigurator::configure(logconf_fname);
      } else {
        PropertyConfigurator::configure(logconf_fname);
      }
      LOG4CXX_DEBUG(logger, "log4cxx config file is set to " << vm["logconfig"].as<std::string>());
    } else {
      BasicConfigurator::configure();
    }

		if (vm.count("threads"))
		{
			cfg.setNum0MQThreads(vm["threads"].as<unsigned int>());
			LOG4CXX_DEBUG(logger, "Setting number of ZeroMQ threads to " << cfg.getNum0MQThreads());
		}

		if (vm.count("consumers"))
		{
			cfg.setNumConsumers(vm["consumers"].as<unsigned int>());
			LOG4CXX_DEBUG(logger, "Setting number of expected consumers to " << cfg.getNumConsumers());
		}

		if (vm.count("fanport"))
		{
			cfg.setFanChannelPortStart(vm["fanport"].as<unsigned int>());
			LOG4CXX_DEBUG(logger, "Setting fan channel port to " << cfg.getFanChannelPortStart());
		}

		if (vm.count("ctrlport"))
		{
			cfg.setCtrlChannelPort(vm["ctrlport"].as<std::string>());
			LOG4CXX_DEBUG(logger, "Setting control channel port to " << cfg.getCtrlChannelPort());
		}

		if (vm.count("addr"))
		{
			cfg.setEigerChannelAddress(vm["addr"].as<std::string>());
			LOG4CXX_DEBUG(logger, "Setting Eiger stream address to " << cfg.getEigerChannelAddress());
		}

		if (vm.count("sockets"))
		{
			cfg.setNum0MQSockets(vm["sockets"].as<unsigned int>());
			LOG4CXX_DEBUG(logger, "Setting number of ZeroMQ sockets to " << cfg.getNum0MQSockets());
		}

		if (vm.count("blocksize"))
		{
			cfg.setBlockSize(vm["blocksize"].as<unsigned int>());
			LOG4CXX_DEBUG(logger, "Setting block size to " << cfg.getBlockSize());
		}

	}
	catch (Exception &e)
	{
		LOG4CXX_ERROR(logger, "Got Log4CXX exception: " << e.what());
		rc = 1;
	}
	catch (...)
	{
		LOG4CXX_ERROR(logger, "Exception of unknown type!");
		rc = 1;
	}

	return rc;

}

