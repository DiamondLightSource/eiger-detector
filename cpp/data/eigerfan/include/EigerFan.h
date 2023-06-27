//
// Created by Ulrik Pedersen on 23/03/17.
//

#ifndef EIGERDAQ_EIGERFAN_H
#define EIGERDAQ_EIGERFAN_H

#define RAPIDJSON_HAS_STDSTRING 1

#include <vector>
#include "EigerFanConfig.h"
#include "EigerDefinitions.h"
#include "zmq/zmq.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <log4cxx/logger.h>
#include <boost/shared_ptr.hpp>

#include "stream2.h"

class EigerFan {

  typedef struct
  {
    int connected;
    boost::shared_ptr<zmq::socket_t> sendSocket;
  } EigerConsumer;

public:
  EigerFan();
  EigerFan(EigerFanConfig config_);
  virtual ~EigerFan();
  void run();
  void Stop();
  void SetNumberOfConsumers(int number);

protected:
  void HandleStreamMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket);
  void HandleStartMessage(struct stream2_start_msg* message);
  void HandleImageMessage(struct stream2_image_msg* message);
  void HandleEndMessage(struct stream2_end_msg* message);

  void HandleMonitorMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket, int rank);
  void HandleForwardMonitorMessage(zmq::message_t &message, zmq::socket_t &socket);
  void HandleControlMessage(zmq::message_t &message, zmq::message_t &idMessage);

  void SendMessageToAllConsumers(zmq::message_t &message, int flags = 0);
  void SendMessageToSingleConsumer(zmq::message_t &message, int flags = 0);
  void SendFabricatedEndMessage();

  int GetNumberOfConnectedConsumers();
  bool ExpectedConsumersConnected();

private:
  log4cxx::LoggerPtr log;
  rapidjson::Document jsonDocument;
  EigerFanConfig config;
  zmq::context_t ctx_;
  zmq::socket_t controlSocket;
  zmq::socket_t forwardSocket;
  std::vector<EigerConsumer> consumers;

  bool killRequested;
  Eiger::EigerFanState state;
  int currentSeries;
  int currentConsumerIndexToSendTo;
  uint64_t lastFrameSent;
  uint64_t num_frames_sent;
  std::vector<uint64_t> num_frames_consumed;
  int configuredOffset;
  int currentOffset;
  int numConnectedForwardingSockets;
  bool forwardStream;
};

#endif //EIGERDAQ_EIGERFAN_H
