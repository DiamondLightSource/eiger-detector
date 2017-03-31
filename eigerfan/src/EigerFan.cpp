/*
 * EigerFan.cpp
 *
 *  Created on: 27 Mar 2017
 *      Author: vtu42223
 */
#include <iostream>
#include <string>
#include <sstream>
#include "EigerFan.h"

// Constants
const std::string EigerFan::STREAM_PORT_NUMBER = "9999";

const int EigerFan::DEFAULT_NUM_CONSUMERS = 1;
const std::string EigerFan::DEFAULT_FAN_PORT_NUMBER = "5557";
const std::string EigerFan::DEFAULT_CONTROL_PORT_NUMBER = "5559";
const std::string EigerFan::DEFAULT_STREAM_ADDRESS = "localhost";

const char* EigerFan::HEADER_TYPE_KEY = "htype";
const char* EigerFan::HEADER_DETAIL_KEY = "header_detail";

const std::string EigerFan::GLOBAL_HEADER_TYPE = "dheader-1.0";
const std::string EigerFan::IMAGE_HEADER_TYPE = "dimage-1.0";
const std::string EigerFan::END_HEADER_TYPE = "dseries_end-1.0";

const std::string EigerFan::HEADER_DETAIL_ALL = "all";
const std::string EigerFan::HEADER_DETAIL_BASIC = "basic";
const std::string EigerFan::HEADER_DETAIL_NONE = "none";

const int EigerFan::MORE_MESSAGES = 1;

const std::string EigerFan::CONTROL_KILL = "KILL";
const std::string EigerFan::CONTROL_STATUS = "STATUS";
const std::string EigerFan::CONTROL_CLOSE = "CLOSE";

// Utility variables
int more;
size_t more_size = sizeof (more);

std::string GetStateString(EigerFanState state) {
	switch(state) {
		case WAITING_CONSUMERS:	return STATE_WAITING_CONSUMERS;
		case WAITING_STREAM: 	return STATE_WAITING_STREAM;
		case DSTR_HEADER: 		return STATE_DSTR_HEADER;
		case DSTR_IMAGE:  		return STATE_DSTR_IMAGE;
		case DSTR_END:  		return STATE_DSTR_END;
		case KILL_REQUESTED:	return STATE_KILL_REQUESTED;
	}
	return "UNKNOWN STATE";
}

EigerFan::EigerFan()
: logger(log4cxx::Logger::getLogger("EigerFan")),
		ctx_(1),
		sendSocket(ctx_, ZMQ_PUSH),
		recvSocket(ctx_, ZMQ_PULL),
		controlSocket(ctx_, ZMQ_REP) {
	fanPortNumber = DEFAULT_FAN_PORT_NUMBER;
	controlPortNumber = DEFAULT_CONTROL_PORT_NUMBER;
	streamAddress = DEFAULT_STREAM_ADDRESS;
	numExpectedConsumers = DEFAULT_NUM_CONSUMERS;
	numConnectedConsumers = 0;
	killRequested = false;
	state = WAITING_CONSUMERS;
}

EigerFan::~EigerFan() {
}

void EigerFan::Start() {
	logger->info("Starting EigerFan");
	int linger = 0;

	// Setup Control socket
	std::string controlAddress("tcp://*:");
	controlAddress.append(controlPortNumber);
	logger->info(std::string("Binding control address to").append(controlAddress));
	controlSocket.bind (controlAddress.c_str());
	controlSocket.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

	// Setup Fan Send Socket
	std::string fanAddress("tcp://*:");
	fanAddress.append(fanPortNumber);
	logger->info(std::string("Binding fan send address to").append(fanAddress));
	sendSocket.bind(fanAddress.c_str());
	sendSocket.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

	// Setup monitor for Fan Send Socket to listen to accepted and disconnected events
	void *sendSocketPtr = sendSocket;
	int rc = zmq_socket_monitor(sendSocketPtr, "inproc://monitor-sender", ZMQ_EVENT_ACCEPTED | ZMQ_EVENT_DISCONNECTED);
	if (rc < 0) {
		std::ostringstream oss;
		oss << "Error setting up monitor. 0MQ Error number: " << zmq_errno();
		logger->error(oss.str());
		return;
	}
	zmq::socket_t monitorSocket(ctx_, ZMQ_PAIR);
	monitorSocket.connect("inproc://monitor-sender");
	monitorSocket.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

	// Wait for configured number of consumers to connect
	logger->info("Waiting for Consumers");

	//  Initialise pre-run poll set
	zmq::pollitem_t preRunPollItems [] = {
		{ controlSocket, 0, ZMQ_POLLIN, 0 },
		{ monitorSocket, 0, ZMQ_POLLIN, 0 }
	};

	while (numConnectedConsumers < numExpectedConsumers && killRequested != true) {
		zmq::message_t pollMessage;
		zmq::poll (&preRunPollItems [0], 2, -1);

		if (preRunPollItems [0].revents & ZMQ_POLLIN) {
			controlSocket.recv(&pollMessage);
			HandleControlMessage(pollMessage);
		}

		if (preRunPollItems [1].revents & ZMQ_POLLIN) {
			monitorSocket.recv(&pollMessage);
			HandleMonitorMessage(pollMessage, monitorSocket);
		}
	}

	if (killRequested) {
		logger->warn("Kill was requested before all consumers had joined");
		return;
	}

	logger->info("All expected consumers connected. Connecting to Eiger Stream");

	std::string streamConnectionAddress("tcp://");
	streamConnectionAddress.append(streamAddress);
	streamConnectionAddress.append(":");
	streamConnectionAddress.append(STREAM_PORT_NUMBER);
	logger->info(std::string("Connecting to stream address at ").append(streamConnectionAddress));
	recvSocket.connect(streamConnectionAddress.c_str());
	recvSocket.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

	//  Initialise run poll set
	zmq::pollitem_t runPollItems [] = {
		{ controlSocket, 0, ZMQ_POLLIN, 0 },
		{ monitorSocket, 0, ZMQ_POLLIN, 0 },
		{ recvSocket, 0, ZMQ_POLLIN, 0 }
	};

	state = WAITING_STREAM;

	//  Process tasks forever or until kill is requested
	while (killRequested != true) {
		zmq::message_t message;
		zmq::poll (&runPollItems [0], 3, -1);

		// Control socket events
		if (runPollItems [0].revents & ZMQ_POLLIN) {
			controlSocket.recv(&message);
			HandleControlMessage(message);
			if (killRequested) {
			//	break;
			}
		}

		// Monitor socket events
		if (runPollItems [1].revents & ZMQ_POLLIN) {
			monitorSocket.recv(&message);
			HandleMonitorMessage(message, monitorSocket);
		}

		// Stream socket events
		if (runPollItems [2].revents & ZMQ_POLLIN) {
			recvSocket.recv(&message);

			HandleStreamMessage(message, recvSocket);
		}
	}
	logger->info("Shutting down EigerFan sockets");
	monitorSocket.close();
	sendSocket.close();
	recvSocket.close();
	controlSocket.close();
}

std::string EigerFan::Stop() {
	std::string result("OK");
	killRequested = true;
	state = KILL_REQUESTED;
	return result;
}

void EigerFan::HandleStreamMessage(zmq::message_t &message, zmq::socket_t &socket) {

	std::string smessage(static_cast<char*>(message.data()), message.size());

	// Interpret the message as a JSON string
	jsonDocument.Parse(smessage.c_str());
	if (jsonDocument.HasParseError()) {
		logger->error("Error parsing stream message into json");
	} else {
		rapidjson::Value& headerTypeValue = jsonDocument[HEADER_TYPE_KEY];
		std::string htype(headerTypeValue.GetString());
		if (htype.compare(GLOBAL_HEADER_TYPE) == 0) {
			HandleGlobalHeaderMessage(message, socket);
		} else if (htype.compare(IMAGE_HEADER_TYPE) == 0) {
			HandleImageDataMessage(message, socket);
		} else if (htype.compare(END_HEADER_TYPE) == 0) {
			HandleEndOfSeriesMessage(message, socket);
		} else {
			logger->error(std::string("Unknown header type").append(htype));
		}
	}

	// Ensure there aren't any leftover messages on the socket
	recvSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		socket.recv(&messagePartExtra);
		logger->error("Unexpected unhandled message in stream");
		return;
	}
}

void EigerFan::HandleGlobalHeaderMessage(zmq::message_t &messagePart1, zmq::socket_t &socket) {
	std::vector<zmq::message_t*> messageList;

	std::string part1json(static_cast<char*>(messagePart1.data()), messagePart1.size());
	logger->info(std::string("Received Global Header message: ").append(part1json));

	jsonDocument.Parse(part1json.c_str());
	if (jsonDocument.HasParseError()) {
		logger->error("Error parsing Global Header message into json");
	} else {
		rapidjson::Value& headerDetailValue = jsonDocument[HEADER_DETAIL_KEY];
		std::string headerDetail(headerDetailValue.GetString());

		if (headerDetail.compare(HEADER_DETAIL_NONE) == 0) {
			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				logger->debug("Header has appendix");
				zmq::message_t messageAppendix;
				socket.recv(&messageAppendix);
				messageList.push_back(&messagePart1);
				messageList.push_back(&messageAppendix);
				SendMessagesToAllConsumers(messageList);
			} else {
				SendMessageToAllConsumers(messagePart1);
			}

		} else if (headerDetail.compare(HEADER_DETAIL_BASIC) == 0) {
			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 1 part but expected 2 for 'basic' detail");
				return; // TODO is it right to return in this case?
			}

			zmq::message_t messagePart2;
			socket.recv(&messagePart2);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				logger->debug("Header has appendix");
				zmq::message_t messageAppendix;
				socket.recv(&messageAppendix);
				messageList.push_back(&messagePart1);
				messageList.push_back(&messagePart2);
				messageList.push_back(&messageAppendix);
				SendMessagesToAllConsumers(messageList);
			} else {
				messageList.push_back(&messagePart1);
				messageList.push_back(&messagePart2);
				SendMessagesToAllConsumers(messageList);
			}

		} else if (headerDetail.compare(HEADER_DETAIL_ALL) == 0) {
			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 1 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 2
			zmq::message_t messagePart2;
			socket.recv(&messagePart2);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 2 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 3
			zmq::message_t messagePart3;
			socket.recv(&messagePart3);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 3 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 4
			zmq::message_t messagePart4;
			socket.recv(&messagePart4);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 4 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 5
			zmq::message_t messagePart5;
			socket.recv(&messagePart5);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 5 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 6
			zmq::message_t messagePart6;
			socket.recv(&messagePart6);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 6 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 7
			zmq::message_t messagePart7;
			socket.recv(&messagePart7);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				logger->error("Header only contained 7 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 8
			zmq::message_t messagePart8;
			socket.recv(&messagePart8);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				logger->debug("Header has appendix");
				zmq::message_t messageAppendix;
				socket.recv(&messageAppendix);
				messageList.push_back(&messagePart1);
				messageList.push_back(&messagePart2);
				messageList.push_back(&messagePart3);
				messageList.push_back(&messagePart4);
				messageList.push_back(&messagePart5);
				messageList.push_back(&messagePart6);
				messageList.push_back(&messagePart7);
				messageList.push_back(&messagePart8);
				messageList.push_back(&messageAppendix);
				SendMessagesToAllConsumers(messageList);
			} else {
				messageList.push_back(&messagePart1);
				messageList.push_back(&messagePart2);
				messageList.push_back(&messagePart3);
				messageList.push_back(&messagePart4);
				messageList.push_back(&messagePart5);
				messageList.push_back(&messagePart6);
				messageList.push_back(&messagePart7);
				messageList.push_back(&messagePart8);
				SendMessagesToAllConsumers(messageList);
			}
		}
		else {
			logger->error("Unexpected header detail type");
		}
	}

	if (state != WAITING_STREAM) {
		logger->warn(std::string("Received Global Header message in unexpected state: ").append(GetStateString(state)));
	}
	state = DSTR_HEADER;
	logger->debug("Finished Handling Header Message");
}

void EigerFan::HandleImageDataMessage(zmq::message_t &messagePart1, zmq::socket_t &socket) {
	logger->debug("Handling Image Data Message");

	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		logger->error("Image Data only contained 1 part");
		return;
	}

	// Part 2 - shape and size
	zmq::message_t messagePart2;
	socket.recv(&messagePart2);

	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		logger->error("Image Data only contained 2 parts");
		return;
	}

	// Part 3 - data blob
	zmq::message_t messagePart3;
	socket.recv(&messagePart3);

	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		logger->error("Image Data only contained 3 parts");
		return;
	}

	//Part 4 - times
	zmq::message_t messagePart4;
	socket.recv(&messagePart4);

	// Handle appendix
	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more == MORE_MESSAGES) {
		logger->debug("Image has appendix");
		zmq::message_t messageAppendix;
		socket.recv(&messageAppendix);

		// Send the data on to a consumer
		sendSocket.send(messagePart1, ZMQ_SNDMORE);
		sendSocket.send(messagePart2, ZMQ_SNDMORE);
		sendSocket.send(messagePart3, ZMQ_SNDMORE);
		sendSocket.send(messagePart4, ZMQ_SNDMORE);
		sendSocket.send(messageAppendix);
	} else {
		// Send the data on to a consumer
		sendSocket.send(messagePart1, ZMQ_SNDMORE);
		sendSocket.send(messagePart2, ZMQ_SNDMORE);
		sendSocket.send(messagePart3, ZMQ_SNDMORE);
		sendSocket.send(messagePart4);
	}

	if (state != DSTR_IMAGE && state != DSTR_HEADER) {
		logger->warn(std::string("Received Image Data message in unexpected state: ").append(GetStateString(state))); // TODO debug instead of warn?
	}
	state = DSTR_IMAGE;
	logger->debug("Finished Handling Image Data Message");
}

void EigerFan::HandleEndOfSeriesMessage(zmq::message_t &message, zmq::socket_t &socket) {
	logger->info("Handling EndOfSeries Message");
	SendMessageToAllConsumers(message);
	if (state != DSTR_IMAGE) {
		logger->warn(std::string("Received EndOfSeries message in unexpected state: ").append(GetStateString(state)));
	}
	state = DSTR_END;
	logger->debug("Finished Handling EndOfSeries Message");
}

void EigerFan::HandleMonitorMessage(zmq::message_t &message, zmq::socket_t &socket) {
	logger->debug("Handling Monitor Message");

	// Get the event code from the message, which is a number contained in the first 16 bits
	uint16_t event = *(uint16_t *) (message.data());

	if (event == ZMQ_EVENT_ACCEPTED) {
		numConnectedConsumers++;
		if (WAITING_CONSUMERS != state) {
			logger->error(std::string("Consumer connected whilst in state: ").append(GetStateString(state)));
		}
	} else if (event == ZMQ_EVENT_DISCONNECTED) {
		numConnectedConsumers--;
		if (WAITING_CONSUMERS != state) {
			logger->error(std::string("Consumer disconnected whilst in state: ").append(GetStateString(state)));
		}
	}

	// Handle any message parts at the end
	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		socket.recv(&messagePartExtra);
		logger->error("Monitor contained more parts than expected");
	}
	logger->debug("Finished Handling Monitor Message");
}

void EigerFan::HandleControlMessage(zmq::message_t &message) {

	std::string command(static_cast<char*>(message.data()), message.size());
	logger->info(std::string("Handling Control Message: ").append(command));

	if (command.compare(CONTROL_KILL) == 0) {
		std::string replyString = Stop();
		zmq::message_t reply (replyString.size());
		memcpy (reply.data (), replyString.c_str(), replyString.size());
		controlSocket.send (reply);
	} else if (command.compare(CONTROL_STATUS) == 0) {

		rapidjson::Document document;
		document.SetObject();

		// Add State
		rapidjson::Value keyState("state", document.GetAllocator());
		rapidjson::Value valueState(GetStateString(state), document.GetAllocator());
		document.AddMember(keyState, valueState, document.GetAllocator());

		// Add Number of connected
		rapidjson::Value keyNumConn("num_conn", document.GetAllocator());
		rapidjson::Value valueNumConn(numConnectedConsumers);
		document.AddMember(keyNumConn, valueNumConn, document.GetAllocator());

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		document.Accept(writer);
		std::string status(buffer.GetString());
		zmq::message_t reply (status.size());
		memcpy (reply.data (), status.c_str(), status.size());
		controlSocket.send (reply);
	} else if (command.compare(CONTROL_CLOSE) == 0) {
		// TODO
	}

	// Handle any message parts at the end
	controlSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		controlSocket.recv(&messagePartExtra);
		logger->error("Control contained more parts than expected");
	}

	logger->debug("Finished Handling Control Message");
}

void EigerFan::SendMessageToAllConsumers(zmq::message_t& message, int flags) {
	std::ostringstream oss;
	oss << "Sending message to all consumers. Number of consumers = " << numConnectedConsumers;
	logger->debug(oss.str());
	// Make as many copies as necessary (number to send minus 1) and send them
	for (int i = 0; i < numConnectedConsumers-1; i++) {
		zmq::message_t messageCopy;
		messageCopy.copy(&message);
		sendSocket.send(messageCopy, flags);
	}
	// Send the actual message for the last one
	sendSocket.send(message, flags);

	logger->debug("Finished Sending message to all consumers");
}

void EigerFan::SendMessagesToAllConsumers(std::vector<zmq::message_t*> &messageList) {
	std::ostringstream oss;
	oss << "Sending multiple messages to all consumers. Number of consumers = " << numConnectedConsumers;
	logger->debug(oss.str());
	int messageListSize = messageList.size();
	// Make as many copies as necessary (number to send minus 1) and send them
	for (int consumerCount = 0; consumerCount < numConnectedConsumers-1; consumerCount++) {
		for (int messageCount = 0; messageCount < messageListSize; messageCount++)
		{
			zmq::message_t messageCopy;
			messageCopy.copy(messageList[messageCount]);
			if (messageCount != messageListSize - 1) {
				sendSocket.send(messageCopy, ZMQ_SNDMORE);
			} else {
				sendSocket.send(messageCopy);
			}
		}
	}
	// Send the actual message for the last one
	for (int messageCount = 0; messageCount < messageListSize; messageCount++)
	{
		if (messageCount != messageListSize - 1) {
			sendSocket.send(*messageList[messageCount], ZMQ_SNDMORE);
		} else {
			sendSocket.send(*messageList[messageCount]);
		}
	}

	messageList.clear();
	logger->debug("Finished Sending multiple messages to all consumers");
}

void EigerFan::SetNumberOfConsumers(int number) {
	numExpectedConsumers = number;
	std::ostringstream oss;
	oss << "Set number of expected Consumers to " << numExpectedConsumers;
	logger->debug(oss.str());
}

void EigerFan::SetFanPortNumber(std::string portNumber) {
	fanPortNumber = portNumber;
	std::ostringstream oss;
	oss << "Set fan port number to " << fanPortNumber;
	logger->debug(oss.str());
}

void EigerFan::SetControlPortNumber(std::string portNumber) {
	controlPortNumber = portNumber;
	std::ostringstream oss;
	oss << "Set control port number to " << controlPortNumber;
	logger->debug(oss.str());
}

void EigerFan::SetStreamAddress(std::string address) {
	streamAddress = address;
	std::ostringstream oss;
	oss << "Set stream address to " << streamAddress;
	logger->debug(oss.str());
}


