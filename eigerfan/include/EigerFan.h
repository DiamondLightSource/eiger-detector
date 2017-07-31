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
public:

	EigerFan();
	EigerFan(EigerFanConfig config_);
	virtual ~EigerFan();
	void run();
	void Stop();
	void SetNumberOfConsumers(int number);
protected:
	void HandleStreamMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleGlobalHeaderMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleImageDataMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleEndOfSeriesMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleMonitorMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket, int rank);
	void HandleControlMessage(zmq::message_t &message);
	void SendMessageToAllConsumers(zmq::message_t &message, int flags = 0);
	void SendMessagesToAllConsumers(std::vector<zmq::message_t*> &messageLista);
	void SendMessageToSingleConsumer(zmq::message_t &message, int flags = 0);
	void SendFabricatedEndMessage();
	std::string AddAcquisitionIDToPart1FromAppendix(zmq::message_t& messageAppendix);
	std::string AddAcquisitionIDToPart1(std::string acquisitionID);

private:
	log4cxx::LoggerPtr log;
	rapidjson::Document jsonDocument;
	EigerFanConfig config;
	zmq::context_t ctx_;
	zmq::socket_t recvSocket;
	zmq::socket_t controlSocket;
	std::vector<boost::shared_ptr<zmq::socket_t> > sendSockets;

	int numConnectedConsumers;
	bool killRequested;
	Eiger::EigerFanState state;
	int currentSeries;
	int currentConsumerIndexToSendTo;
	std::string currentAcquisitionID;
	uint64_t lastFrameSent;
	int configuredOffset;
	int currentOffset;
};


#endif //EIGERDAQ_EIGERFAN_H
