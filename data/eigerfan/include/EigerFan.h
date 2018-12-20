//
// Created by up45 on 23/03/17.
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
  void HandleGlobalHeaderMessage(boost::shared_ptr<zmq::socket_t> socket);
  void HandleImageDataMessage(boost::shared_ptr<zmq::socket_t> socket);
  void HandleEndOfSeriesMessage(boost::shared_ptr<zmq::socket_t> socket);
  void HandleMonitorMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket, int rank);
  void HandleForwardMonitorMessage(zmq::message_t &message, zmq::socket_t &socket);
  void HandleControlMessage(zmq::message_t &message, zmq::message_t &idMessage);
  void SendMessageToAllConsumers(zmq::message_t &message, int flags = 0);
  void SendMessagesToAllConsumers(std::vector<zmq::message_t*> &messageLista);
  void SendMessageToSingleConsumer(zmq::message_t &message, int flags = 0);
  void SendFabricatedEndMessage();
  std::string AddAcquisitionIDToPart1();
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
  std::string configuredAcquisitionID;
  std::string currentAcquisitionID;
  uint64_t lastFrameSent;
  uint64_t num_frames_sent;
  int configuredOffset;
  int currentOffset;
  int numConnectedForwardingSockets;
};


#endif //EIGERDAQ_EIGERFAN_H
