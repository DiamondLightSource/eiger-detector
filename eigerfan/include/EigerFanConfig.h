/*
 * EigerFanConfig.h
 *
 *  Created on: 6 Apr 2017
 *      Author: vtu42223
 */

#ifndef EIGERFAN_INCLUDE_EIGERFANCONFIG_H_
#define EIGERFAN_INCLUDE_EIGERFANCONFIG_H_

namespace EigerFanDefaults {
	const int DEFAULT_NUM_THREADS = 2;
	const int DEFAULT_NUM_CONSUMERS = 1;
	const std::string DEFAULT_FAN_PORT_NUMBER = "5557";
	const std::string DEFAULT_CONTROL_PORT_NUMBER = "5559";
	const std::string DEFAULT_STREAM_ADDRESS = "localhost";
	const int DEFAULT_FAN_PORT_NUMBER_START = 31600;
}

class EigerFanConfig
	{
	public:

	EigerFanConfig() :
		num_zmq_threads(EigerFanDefaults::DEFAULT_NUM_THREADS),
		num_consumers(EigerFanDefaults::DEFAULT_NUM_CONSUMERS),
		ctrl_channel_port(EigerFanDefaults::DEFAULT_CONTROL_PORT_NUMBER),
		fan_channel_port(EigerFanDefaults::DEFAULT_FAN_PORT_NUMBER),
		eiger_channel_address(EigerFanDefaults::DEFAULT_STREAM_ADDRESS),
		fan_channel_port_start(EigerFanDefaults::DEFAULT_FAN_PORT_NUMBER_START)
		{
		};

	void setEigerChannelAddress(const std::string& eigerChannelAddress) {
		eiger_channel_address = eigerChannelAddress;
	}

	void setCtrlChannelPort(const std::string& ctrlChannelPort) {
		ctrl_channel_port = ctrlChannelPort;
	}

	void setFanChannelPort(const std::string& fanChannelPort) {
		fan_channel_port = fanChannelPort;
	}

	void setNumConsumers(int numConsumers) {
		num_consumers = numConsumers;
	}

	void setNum0MQThreads(int numZmqThreads) {
		num_zmq_threads = numZmqThreads;
	}

	void setFanChannelPortStart(int fanChannelPortStart) {
		fan_channel_port_start = fanChannelPortStart;
	}

	const std::string& getCtrlChannelPort() const {
		return ctrl_channel_port;
	}

	const std::string& getEigerChannelAddress() const {
		return eiger_channel_address;
	}

	const std::string& getFanChannelPort() const {
		return fan_channel_port;
	}

	int getNumConsumers() const {
		return num_consumers;
	}

	int getNum0MQThreads() const {
		return num_zmq_threads;
	}

	int getFanChannelPortStart() const {
		return fan_channel_port_start;
	}

	private:

		int                   num_zmq_threads;    // Number of 0MQ threads
		int                   num_consumers;    // Expected number of consumers
		std::string           ctrl_channel_port;  // Port to bind to for the control channel
		std::string           fan_channel_port;  // Port to bind to for the fan channel
		std::string           eiger_channel_address;  // Address to connect to for the Eiger Stream
		int           		  fan_channel_port_start;  // Port to bind to for the fan channel

		friend class EigerFan;
	};

#endif /* EIGERFAN_INCLUDE_EIGERFANCONFIG_H_ */
