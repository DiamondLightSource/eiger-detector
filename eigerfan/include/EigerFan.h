//
// Created by up45 on 23/03/17.
//

#ifndef EIGERDAQ_EIGERFAN_H
#define EIGERDAQ_EIGERFAN_H

#define RAPIDJSON_HAS_STDSTRING 1

#include <vector>
#include "zmq/zmq.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <log4cxx/logger.h>

enum EigerFanState { WAITING_CONSUMERS,WAITING_STREAM,DSTR_HEADER,DSTR_IMAGE,DSTR_END,KILL_REQUESTED};

static const std::string STATE_WAITING_CONSUMERS = "WAITING_CONSUMERS";
static const std::string STATE_WAITING_STREAM = "WAITING_STREAM";
static const std::string STATE_DSTR_HEADER = "DSTR_HEADER";
static const std::string STATE_DSTR_IMAGE = "DSTR_IMAGE";
static const std::string STATE_DSTR_END = "DSTR_END";
static const std::string STATE_KILL_REQUESTED = "KILL_REQUESTED";

class EigerFan {
public:
	static const std::string STREAM_PORT_NUMBER;

	static const int DEFAULT_NUM_CONSUMERS;
	static const std::string DEFAULT_FAN_PORT_NUMBER;
	static const std::string DEFAULT_CONTROL_PORT_NUMBER;
	static const std::string DEFAULT_STREAM_ADDRESS;

	static const char* HEADER_TYPE_KEY;
	static const char* HEADER_DETAIL_KEY;
	static const char* SERIES_KEY;

	static const std::string GLOBAL_HEADER_TYPE;
	static const std::string IMAGE_HEADER_TYPE;
	static const std::string END_HEADER_TYPE;

	static const std::string HEADER_DETAIL_ALL;
	static const std::string HEADER_DETAIL_BASIC;
	static const std::string HEADER_DETAIL_NONE;

	static const int MORE_MESSAGES;

	static const std::string CONTROL_KILL;
	static const std::string CONTROL_STATUS;
	static const std::string CONTROL_CLOSE;

	static const std::string END_STREAM_MESSAGE;

	EigerFan();
	virtual ~EigerFan();
	void run();
	std::string Stop();
	void SetNumberOfConsumers(int number);
	void SetFanPortNumber(std::string portNumber);
	void SetControlPortNumber(std::string portNumber);
	void SetStreamAddress(std::string address);
protected:
	void HandleStreamMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleGlobalHeaderMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleImageDataMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleEndOfSeriesMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleMonitorMessage(zmq::message_t &message, zmq::socket_t &socket);
	void HandleControlMessage(zmq::message_t &message);
	void SendMessageToAllConsumers(zmq::message_t &message, int flags = 0);
	void SendMessagesToAllConsumers(std::vector<zmq::message_t*> &messageLista);
	void SendFabricatedEndMessage();

private:
	log4cxx::LoggerPtr log;
	rapidjson::Document jsonDocument;
	zmq::context_t ctx_;
	zmq::socket_t sendSocket;
	zmq::socket_t recvSocket;
	zmq::socket_t controlSocket;

	int numExpectedConsumers;
	std::string fanPortNumber;
	std::string controlPortNumber;
	std::string streamAddress;
	int numConnectedConsumers;
	bool killRequested;
	EigerFanState state;
	int currentSeries;
};


#endif //EIGERDAQ_EIGERFAN_H
