/*
 * EigerFanConfig.h
 *
 *  Created on: 6 Apr 2017
 *      Author: Matt Taylor
 */

#ifndef EIGERFAN_INCLUDE_EIGERFANCONFIG_H_
#define EIGERFAN_INCLUDE_EIGERFANCONFIG_H_

#include "EigerDefinitions.h"

namespace EigerFanDefaults {
  const int DEFAULT_NUM_THREADS = 2;
  const int DEFAULT_NUM_CONSUMERS = 1;
  const std::string DEFAULT_CONTROL_PORT_NUMBER = "5559";
  const std::string DEFAULT_STREAM_ADDRESS = "localhost";
  const std::string DEFAULT_EIGER_PORT_NUMBER = Eiger::STREAM_PORT_NUMBER;
  const int DEFAULT_FAN_PORT_NUMBER_START = 31600;
  const int DEFAULT_NUM_CONTEXT_THREADS = 1;
  const int DEFAULT_BLOCK_SIZE = 1;
  const std::string DEFAULT_FORWARD_PORT_NUMBER = "9009";
}

class EigerFanConfig
{
public:

  EigerFanConfig() :
    num_threads(EigerFanDefaults::DEFAULT_NUM_THREADS),
    num_consumers(EigerFanDefaults::DEFAULT_NUM_CONSUMERS),
    ctrl_channel_port(EigerFanDefaults::DEFAULT_CONTROL_PORT_NUMBER),
    eiger_channel_address(EigerFanDefaults::DEFAULT_STREAM_ADDRESS),
    eiger_channel_port(EigerFanDefaults::DEFAULT_EIGER_PORT_NUMBER),
    forward_channel_port(EigerFanDefaults::DEFAULT_FORWARD_PORT_NUMBER),
    fan_channel_port_start(EigerFanDefaults::DEFAULT_FAN_PORT_NUMBER_START),
    num_zmq_context_threads(EigerFanDefaults::DEFAULT_NUM_CONTEXT_THREADS),
    block_size(EigerFanDefaults::DEFAULT_BLOCK_SIZE)
    {
    };

  void setEigerChannelAddress(const std::string& eigerChannelAddress) {
    eiger_channel_address = eigerChannelAddress;
  }

  void setCtrlChannelPort(const std::string& ctrlChannelPort) {
    ctrl_channel_port = ctrlChannelPort;
  }

  void setNumConsumers(int numConsumers) {
    num_consumers = numConsumers;
  }

  void setNumThreads(int numThreads) {
    num_threads = numThreads;
  }

  void setFanChannelPortStart(int fanChannelPortStart) {
    fan_channel_port_start = fanChannelPortStart;
  }

  void setNum0MQContextThreads(int numZmqContextThreads) {
    num_zmq_context_threads = numZmqContextThreads;
  }

  void setBlockSize(int blockSize) {
    block_size = blockSize;
  }

  void setEigerChannelPort(const std::string& eigerPort) {
    eiger_channel_port = eigerPort;
  }

  const std::string& getCtrlChannelPort() const {
    return ctrl_channel_port;
  }

  const std::string& getEigerChannelAddress() const {
    return eiger_channel_address;
  }

  int getNumConsumers() const {
    return num_consumers;
  }

  int getNumThreads() const {
    return num_threads;
  }

  int getFanChannelPortStart() const {
    return fan_channel_port_start;
  }

  int getNum0MQContextThreads() const {
    return num_zmq_context_threads;
  }

  int getBlockSize() const {
    return block_size;
  }

  const std::string& getEigerChannelPort() const {
    return eiger_channel_port;
  }

private:

  int                   num_threads;    // Number of 0MQ threads
  int                   num_consumers;    // Expected number of consumers
  std::string           ctrl_channel_port;  // Port to bind to for the control channel
  std::string           eiger_channel_address;  // Address to connect to for the Eiger Stream
  std::string           eiger_channel_port;  // Port to connect to for the Eiger Stream
  std::string           forward_channel_port;  // Port to bind to for the forwarding channel
  int                   fan_channel_port_start;  // Port to bind to for the fan channel
  int                   num_zmq_context_threads;    // Number of 0MQ context threads
  int                   block_size;    // Block Size being used by the downstream data file writers

  friend class EigerFan;
};

#endif /* EIGERFAN_INCLUDE_EIGERFANCONFIG_H_ */
