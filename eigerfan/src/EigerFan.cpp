//
// Created by up45 on 23/03/17.
//
#include <iostream>
#include <string>
#include <sstream>
#include "EigerFan.h"

// Utility variables
int more;
size_t more_size = sizeof (more);

using namespace Eiger;

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
: ctx_(EigerFanDefaults::DEFAULT_NUM_THREADS),
  recvSocket(ctx_, ZMQ_PULL),
  controlSocket(ctx_, ZMQ_REP) {
	this->log = log4cxx::Logger::getLogger("ED.EigerFan");
	LOG4CXX_INFO(log, "Creating EigerFan object from default options");
	numConnectedConsumers = 0;
	killRequested = false;
	state = WAITING_CONSUMERS;
	currentSeries = 0;
	currentConsumerIndexToSendTo = 0;
	lastFrameSent = 0;
}

EigerFan::EigerFan(EigerFanConfig config_)
: ctx_(config_.num_zmq_threads),
  recvSocket(ctx_, ZMQ_PULL),
  controlSocket(ctx_, ZMQ_REP) {
	this->log = log4cxx::Logger::getLogger("ED.EigerFan");
	config = config_;
	LOG4CXX_INFO(log, "Creating EigerFan object from config options");
	numConnectedConsumers = 0;
	killRequested = false;
	state = WAITING_CONSUMERS;
	currentSeries = 0;
	currentConsumerIndexToSendTo = 0;
	lastFrameSent = 0;
}

EigerFan::~EigerFan() {
}

void EigerFan::run() {
    LOG4CXX_INFO(log, "EigerFan::run()");
	LOG4CXX_INFO(log, "Starting EigerFan");
	int linger = 0;

	// Setup Control socket
	std::string controlAddress("tcp://*:");
	controlAddress.append(config.ctrl_channel_port);
	LOG4CXX_INFO(log, std::string("Binding control address to ").append(controlAddress));
	controlSocket.bind (controlAddress.c_str());
	controlSocket.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

	// Setup Fan Send Sockets
	for (int i = 0; i < config.num_consumers; i++) {
		std::ostringstream fanAddress;
		int port = config.fan_channel_port_start + i;
		fanAddress << "tcp://*:" << port;

		LOG4CXX_INFO(log, std::string("Binding fan send address to ").append(fanAddress.str()));
		int sndHwmSet = 0;

		boost::shared_ptr<zmq::socket_t> sendSocket(new zmq::socket_t(ctx_, ZMQ_PUSH));
		sendSocket->setsockopt (ZMQ_SNDHWM, &sndHwmSet, sizeof (sndHwmSet));
		sendSocket->bind(fanAddress.str().c_str());
		sendSocket->setsockopt (ZMQ_LINGER, &linger, sizeof (linger));
		sendSockets.push_back(sendSocket);
	}

	std::vector<boost::shared_ptr<zmq::socket_t> > monitorSockets;
	for (int i = 0; i < config.num_consumers; i++) {
		std::ostringstream monitorAddress;
		int port = config.fan_channel_port_start + i;
		monitorAddress << "inproc://monitor-sender" << port;

		// Setup monitor for Fan Send Socket to listen to accepted and disconnected events

		zmq::socket_t* socketToMonitor = sendSockets[i].get();

		int rc = zmq_socket_monitor(*socketToMonitor, monitorAddress.str().c_str(), ZMQ_EVENT_ACCEPTED | ZMQ_EVENT_DISCONNECTED);
		if (rc < 0) {
			LOG4CXX_ERROR(log, "Error setting up monitor. 0MQ Error number: " << zmq_errno());
			return;
		}
		boost::shared_ptr<zmq::socket_t> monitorSocket(new zmq::socket_t(ctx_, ZMQ_PAIR));
		monitorSocket->connect(monitorAddress.str().c_str());
		monitorSocket->setsockopt (ZMQ_LINGER, &linger, sizeof (linger));
		monitorSockets.push_back(monitorSocket);
	}

	// Wait for configured number of consumers to connect
	LOG4CXX_INFO(log, "Waiting for Consumers");

	//  Initialise pre-run poll set
	zmq::pollitem_t preRunPollItems [config.num_consumers + 1];
	zmq_pollitem_t controlPollItem;
	controlPollItem.socket = controlSocket;
	controlPollItem.fd = 0;
	controlPollItem.events = ZMQ_POLLIN;
	controlPollItem.revents = 0;
	preRunPollItems[0] = controlPollItem;

	for (int i = 0; i < config.num_consumers; i++) {
		zmq_pollitem_t monitorPollItem;
		zmq::socket_t* monitorSocket = monitorSockets[i].get();
		monitorPollItem.socket = *monitorSocket;
		monitorPollItem.fd = 0;
		monitorPollItem.events = ZMQ_POLLIN;
		monitorPollItem.revents = 0;
		preRunPollItems[i+1] = monitorPollItem;
	}

	while (numConnectedConsumers < config.num_consumers && killRequested != true) {
		zmq::message_t pollMessage;
		zmq::poll (&preRunPollItems [0], config.num_consumers + 1, -1);

		if (preRunPollItems [0].revents & ZMQ_POLLIN) {
			controlSocket.recv(&pollMessage);
			HandleControlMessage(pollMessage);
		}

		for (int i = 0; i < config.num_consumers; i++) {
			if (preRunPollItems [i+1].revents & ZMQ_POLLIN) {
				monitorSockets[i]->recv(&pollMessage);
				HandleMonitorMessage(pollMessage, monitorSockets[i]);
			}
		}
	}

	if (killRequested) {
		LOG4CXX_INFO(log, "Kill was requested before all consumers had joined");
		for (int i = 0; i < config.num_consumers; i++) {
			monitorSockets[i]->close();
			sendSockets[i]->close();
		}
		controlSocket.close();
		return;
	}

	std::ostringstream oss;
	oss << "All " << numConnectedConsumers << " expected consumers connected. Connecting to Eiger Stream";

	LOG4CXX_INFO(log, oss.str());

	std::string streamConnectionAddress("tcp://");
	streamConnectionAddress.append(config.eiger_channel_address);
	streamConnectionAddress.append(":");
	streamConnectionAddress.append(STREAM_PORT_NUMBER);
	LOG4CXX_INFO(log, std::string("Connecting to stream address at ").append(streamConnectionAddress));
	recvSocket.connect(streamConnectionAddress.c_str());
	recvSocket.setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

	//  Initialise run poll set
	zmq::pollitem_t runPollItems [config.num_consumers + 2];
	zmq_pollitem_t controlRunPollItem;
	controlRunPollItem.socket = controlSocket;
	controlRunPollItem.fd = 0;
	controlRunPollItem.events = ZMQ_POLLIN;
	controlRunPollItem.revents = 0;
	runPollItems[0] = controlRunPollItem;

	for (int i = 0; i < config.num_consumers; i++) {
		zmq_pollitem_t monitorPollItem;
		zmq::socket_t* monitorSocket = monitorSockets[i].get();
		monitorPollItem.socket = *monitorSocket;
		monitorPollItem.fd = 0;
		monitorPollItem.events = ZMQ_POLLIN;
		monitorPollItem.revents = 0;
		runPollItems[i+1] = monitorPollItem;
	}

	zmq_pollitem_t recvRunPollItem;
	recvRunPollItem.socket = recvSocket;
	recvRunPollItem.fd = 0;
	recvRunPollItem.events = ZMQ_POLLIN;
	recvRunPollItem.revents = 0;
	runPollItems[config.num_consumers + 2 - 1] = recvRunPollItem;

	state = WAITING_STREAM;

	//  Process tasks forever or until kill is requested
	while (killRequested != true) {
		zmq::message_t message;
		zmq::poll (&runPollItems [0], config.num_consumers + 2, -1);

		// Control socket events
		if (runPollItems [0].revents & ZMQ_POLLIN) {
			controlSocket.recv(&message);
			HandleControlMessage(message);
			if (killRequested) {
				break;
			}
		}

		// Monitor socket events
		for (int i = 0; i < config.num_consumers; i++) {
			if (runPollItems [i+1].revents & ZMQ_POLLIN) {
				monitorSockets[i]->recv(&message);
				HandleMonitorMessage(message, monitorSockets[i]);
			}
		}

		// Stream socket events
		if (runPollItems [config.num_consumers + 2 - 1].revents & ZMQ_POLLIN) {
			recvSocket.recv(&message);
			HandleStreamMessage(message, recvSocket);
		}
	}

	LOG4CXX_INFO(log, "Shutting down EigerFan sockets");
	for (int i = 0; i < config.num_consumers; i++) {
		monitorSockets[i]->close();
		sendSockets[i]->close();
	}

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

	try {
		std::string smessage(static_cast<char*>(message.data()), message.size());

		// Interpret the message as a JSON string
		jsonDocument.Parse(smessage.c_str());
		if (jsonDocument.HasParseError()) {
			LOG4CXX_ERROR(log, "Error parsing stream message into json");
		} else {
			rapidjson::Value& headerTypeValue = jsonDocument[HEADER_TYPE_KEY.c_str()];
			std::string htype(headerTypeValue.GetString());
			if (htype.compare(GLOBAL_HEADER_TYPE) == 0) {
				HandleGlobalHeaderMessage(message, socket);
			} else if (htype.compare(IMAGE_HEADER_TYPE) == 0) {
				rapidjson::Value& frameValue = jsonDocument[FRAME_KEY.c_str()];
				int64_t frame(frameValue.GetInt64());
				currentConsumerIndexToSendTo = frame % config.num_consumers;
				HandleImageDataMessage(message, socket);
				lastFrameSent = frame;
			} else if (htype.compare(END_HEADER_TYPE) == 0) {
				HandleEndOfSeriesMessage(message, socket);
				state = WAITING_STREAM;
			} else {
				LOG4CXX_ERROR(log, std::string("Unknown header type ").append(htype));
			}
		}
	}
    catch (std::exception& e)
    {
      LOG4CXX_ERROR(log, "Generic exception handling stream message:\n" << e.what());
    }
	catch (...)
    {
        LOG4CXX_ERROR(log, "Unexpected exception handling stream message");
    }

	// Ensure there aren't any leftover messages on the socket
	recvSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		socket.recv(&messagePartExtra);
		LOG4CXX_ERROR(log, "Unexpected unhandled message in stream");
		recvSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	}
}

void EigerFan::HandleGlobalHeaderMessage(zmq::message_t &messagePart1, zmq::socket_t &socket) {
	std::vector<zmq::message_t*> messageList;

	std::string part1json(static_cast<char*>(messagePart1.data()), messagePart1.size());
	LOG4CXX_INFO(log, std::string("Received Global Header message: ").append(part1json));

	jsonDocument.Parse(part1json.c_str());
	if (jsonDocument.HasParseError()) {
		LOG4CXX_ERROR(log, "Error parsing Global Header message into json");
	} else {
		rapidjson::Value& seriesValue = jsonDocument[SERIES_KEY.c_str()];
		currentSeries = seriesValue.GetInt();

		rapidjson::Value& headerDetailValue = jsonDocument[HEADER_DETAIL_KEY.c_str()];
		std::string headerDetail(headerDetailValue.GetString());

		if (headerDetail.compare(HEADER_DETAIL_NONE) == 0) {
			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				LOG4CXX_DEBUG(log, "Header has appendix");
				zmq::message_t messageAppendix;
				socket.recv(&messageAppendix);
				// Add the Acquisition ID from the appendix to part 1 for easier downstream processing
				std::string part1WithAcquisitionID = AddAcquisitionIDToPart1FromAppendix(messageAppendix);
				zmq::message_t newPart1message(part1WithAcquisitionID.size());
				memcpy (newPart1message.data (), part1WithAcquisitionID.c_str(), part1WithAcquisitionID.size());
				messageList.push_back(&newPart1message);
				messageList.push_back(&messageAppendix);
				SendMessagesToAllConsumers(messageList);
			} else {
				SendMessageToAllConsumers(messagePart1);
			}

		} else if (headerDetail.compare(HEADER_DETAIL_BASIC) == 0) {
			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 1 part but expected 2 for 'basic' detail");
				return; // TODO is it right to return in this case?
			}

			zmq::message_t messagePart2;
			socket.recv(&messagePart2);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				LOG4CXX_DEBUG(log, "Header has appendix");
				zmq::message_t messageAppendix;
				socket.recv(&messageAppendix);
				// Add the Acquisition ID from the appendix to part 1 for easier downstream processing
				std::string part1WithAcquisitionID = AddAcquisitionIDToPart1FromAppendix(messageAppendix);
				zmq::message_t newPart1message(part1WithAcquisitionID.size());
				memcpy (newPart1message.data (), part1WithAcquisitionID.c_str(), part1WithAcquisitionID.size());
				messageList.push_back(&newPart1message);
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
				LOG4CXX_ERROR(log, "Header only contained 1 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 2
			zmq::message_t messagePart2;
			socket.recv(&messagePart2);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 2 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 3
			zmq::message_t messagePart3;
			socket.recv(&messagePart3);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 3 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 4
			zmq::message_t messagePart4;
			socket.recv(&messagePart4);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 4 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 5
			zmq::message_t messagePart5;
			socket.recv(&messagePart5);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 5 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 6
			zmq::message_t messagePart6;
			socket.recv(&messagePart6);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 6 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 7
			zmq::message_t messagePart7;
			socket.recv(&messagePart7);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 7 part but expected 8 for 'all' detail");
				return; // TODO is it right to return in this case?
			}

			// Part 8
			zmq::message_t messagePart8;
			socket.recv(&messagePart8);

			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				LOG4CXX_DEBUG(log, "Header has appendix");
				zmq::message_t messageAppendix;
				socket.recv(&messageAppendix);
				// Add the Acquisition ID from the appendix to part 1 for easier downstream processing
				std::string part1WithAcquisitionID = AddAcquisitionIDToPart1FromAppendix(messageAppendix);
				zmq::message_t newPart1message(part1WithAcquisitionID.size());
				memcpy (newPart1message.data (), part1WithAcquisitionID.c_str(), part1WithAcquisitionID.size());
				messageList.push_back(&newPart1message);
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
			LOG4CXX_ERROR(log, "Unexpected header detail type");
		}
	}

	if (state != WAITING_STREAM) {
		LOG4CXX_WARN(log, std::string("Received Global Header message in unexpected state: ").append(GetStateString(state)));
	}
	state = DSTR_HEADER;
	LOG4CXX_DEBUG(log, "Finished Handling Header Message");
}

void EigerFan::HandleImageDataMessage(zmq::message_t &messagePart1, zmq::socket_t &socket) {
	LOG4CXX_DEBUG(log, "Handling Image Data Message");

	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		LOG4CXX_ERROR(log, "Image Data only contained 1 part");
		return;
	}

	// Add the current Acquisition ID to part 1 for easier downstream processing
	std::string part1WithAcquisitionID = AddAcquisitionIDToPart1(currentAcquisitionID);
	zmq::message_t newPart1message(part1WithAcquisitionID.size());
	memcpy (newPart1message.data (), part1WithAcquisitionID.c_str(), part1WithAcquisitionID.size());

	// Part 2 - shape and size
	zmq::message_t messagePart2;
	socket.recv(&messagePart2);

	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		LOG4CXX_ERROR(log, "Image Data only contained 2 parts");
		return;
	}

	// Part 3 - data blob
	zmq::message_t messagePart3;
	socket.recv(&messagePart3);

	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		LOG4CXX_ERROR(log, "Image Data only contained 3 parts");
		return;
	}

	//Part 4 - times
	zmq::message_t messagePart4;
	socket.recv(&messagePart4);

	// Handle appendix
	socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more == MORE_MESSAGES) {
		LOG4CXX_DEBUG(log, "Image has appendix");
		zmq::message_t messageAppendix;
		socket.recv(&messageAppendix);

		// Send the data on to a consumer
	    SendMessageToSingleConsumer(newPart1message, ZMQ_SNDMORE);
		SendMessageToSingleConsumer(messagePart2, ZMQ_SNDMORE);
		SendMessageToSingleConsumer(messagePart3, ZMQ_SNDMORE);
		SendMessageToSingleConsumer(messagePart4, ZMQ_SNDMORE);
		SendMessageToSingleConsumer(messageAppendix, 0);
	} else {
		// Send the data on to a consumer
	    SendMessageToSingleConsumer(newPart1message, ZMQ_SNDMORE);
		SendMessageToSingleConsumer(messagePart2, ZMQ_SNDMORE);
		SendMessageToSingleConsumer(messagePart3, ZMQ_SNDMORE);
		SendMessageToSingleConsumer(messagePart4, 0);
	}

	if (state != DSTR_IMAGE && state != DSTR_HEADER) {
		LOG4CXX_WARN(log, std::string("Received Image Data message in unexpected state: ").append(GetStateString(state))); // TODO debug instead of warn?
	}
	state = DSTR_IMAGE;
	LOG4CXX_DEBUG(log, "Finished Handling Image Data Message");
}

void EigerFan::HandleEndOfSeriesMessage(zmq::message_t &message, zmq::socket_t &socket) {
	LOG4CXX_INFO(log, "Handling EndOfSeries Message");
	std::string part1WithAcquisitionID = AddAcquisitionIDToPart1(currentAcquisitionID);
	zmq::message_t newPart1message(part1WithAcquisitionID.size());
	memcpy (newPart1message.data (), part1WithAcquisitionID.c_str(), part1WithAcquisitionID.size());
	SendMessageToAllConsumers(newPart1message);
	if (state != DSTR_IMAGE) {
		LOG4CXX_WARN(log, std::string("Received EndOfSeries message in unexpected state: ").append(GetStateString(state)));
	}
	state = DSTR_END;
	LOG4CXX_DEBUG(log, "Finished Handling EndOfSeries Message");
}

void EigerFan::HandleMonitorMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket) {
	LOG4CXX_DEBUG(log, "Handling Monitor Message");

	// Get the event code from the message, which is a number contained in the first 16 bits
	uint16_t event = *(uint16_t *) (message.data());

	if (event == ZMQ_EVENT_ACCEPTED) {
		numConnectedConsumers++;
		if (WAITING_CONSUMERS != state) {
			LOG4CXX_ERROR(log, std::string("Consumer connected whilst in state: ").append(GetStateString(state)));
		} else {
			LOG4CXX_INFO(log, "Consumer connected");
		}
	} else if (event == ZMQ_EVENT_DISCONNECTED) {
		numConnectedConsumers--;
		if (WAITING_CONSUMERS != state) {
			LOG4CXX_ERROR(log, std::string("Consumer disconnected whilst in state: ").append(GetStateString(state)));
		} else {
			LOG4CXX_WARN(log, "Consumer disconnected");
		}
	}

	// Handle any message parts at the end
	socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		socket->recv(&messagePartExtra);
		LOG4CXX_ERROR(log, "Monitor contained more parts than expected");
	}
	LOG4CXX_DEBUG(log, "Finished Handling Monitor Message");
}

void EigerFan::HandleControlMessage(zmq::message_t &message) {

	std::string command(static_cast<char*>(message.data()), message.size());
	LOG4CXX_INFO(log, std::string("Handling Control Message: ").append(command));

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

		// Add Current series
		rapidjson::Value keySeries("series", document.GetAllocator());
		rapidjson::Value valueSeries(currentSeries);
		document.AddMember(keySeries, valueSeries, document.GetAllocator());

		// Add Last Frame sent
		rapidjson::Value keyFrame("frame", document.GetAllocator());
		rapidjson::Value valueFrame(lastFrameSent);
		document.AddMember(keyFrame, valueFrame, document.GetAllocator());

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		document.Accept(writer);
		std::string status(buffer.GetString());
		zmq::message_t reply (status.size());
		memcpy (reply.data (), status.c_str(), status.size());
		controlSocket.send (reply);
	} else if (command.compare(CONTROL_CLOSE) == 0) {
		// Close gracefully - if currently acquiring data, send end of stream message and terminate
		if (state == DSTR_HEADER || state == DSTR_IMAGE) {
			SendFabricatedEndMessage();
		}
		std::string replyString = Stop();
		zmq::message_t reply (replyString.size());
		memcpy (reply.data (), replyString.c_str(), replyString.size());
		controlSocket.send (reply);
	}

	// Handle any message parts at the end
	controlSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		controlSocket.recv(&messagePartExtra);
		LOG4CXX_ERROR(log, "Control contained more parts than expected");
	}

	LOG4CXX_DEBUG(log, "Finished Handling Control Message");
}

void EigerFan::SendMessageToAllConsumers(zmq::message_t& message, int flags) {
	LOG4CXX_DEBUG(log, "Sending message to all consumers. Number of consumers = " << numConnectedConsumers);
	// Make as many copies as necessary (number to send minus 1) and send them
	for (int i = 0; i < numConnectedConsumers-1; i++) {
		zmq::message_t messageCopy;
		messageCopy.copy(&message);
		if (sendSockets.at(i)->send(messageCopy, flags) == false) {
			LOG4CXX_ERROR(log, "Send socket returned false");
		}
	}
	// Send the actual message for the last one
	if (sendSockets.at(numConnectedConsumers-1)->send(message, flags) == false) {
		LOG4CXX_ERROR(log, "Send socket returned false");
	}

	LOG4CXX_DEBUG(log, "Finished Sending message to all consumers");
}

void EigerFan::SendMessagesToAllConsumers(std::vector<zmq::message_t*> &messageList) {
	LOG4CXX_DEBUG(log, "Sending multiple messages to all consumers. Number of consumers = " << numConnectedConsumers);
	int messageListSize = messageList.size();
	// Make as many copies as necessary (number to send minus 1) and send them
	for (int consumerCount = 0; consumerCount < numConnectedConsumers-1; consumerCount++) {
		for (int messageCount = 0; messageCount < messageListSize; messageCount++)
		{
			zmq::message_t messageCopy;
			messageCopy.copy(messageList[messageCount]);
			if (messageCount != messageListSize - 1) {
				if (sendSockets.at(consumerCount)->send(messageCopy, ZMQ_SNDMORE) == false) {
					LOG4CXX_ERROR(log, "Send socket returned false");
				}
			} else {
				if (sendSockets.at(consumerCount)->send(messageCopy) == false) {
					LOG4CXX_ERROR(log, "Send socket returned false");
				}
			}
		}
	}
	// Send the actual message for the last one
	for (int messageCount = 0; messageCount < messageListSize; messageCount++)
	{
		if (messageCount != messageListSize - 1) {
			if (sendSockets.at(numConnectedConsumers-1)->send(*messageList[messageCount], ZMQ_SNDMORE) == false) {
				LOG4CXX_ERROR(log, "Send socket returned false");
			}
		} else {
			if (sendSockets.at(numConnectedConsumers-1)->send(*messageList[messageCount]) == false) {
				LOG4CXX_ERROR(log, "Send socket returned false");
			}
		}
	}

	messageList.clear();
	LOG4CXX_DEBUG(log, "Finished Sending multiple messages to all consumers");
}

void EigerFan::SendMessageToSingleConsumer(zmq::message_t& message, int flags) {
	LOG4CXX_DEBUG(log, "Sending message to single consumers at index:" << currentConsumerIndexToSendTo);

	// Send the message to a consumer
	if (sendSockets.at(currentConsumerIndexToSendTo)->send(message, flags) == false) {
		LOG4CXX_ERROR(log, "Send socket returned false");
	}

	LOG4CXX_DEBUG(log, "Finished Sending message to single consumer");
}

void EigerFan::SendFabricatedEndMessage() {
	LOG4CXX_INFO(log, "Sending Fabricated EndOfSeries Message");

	rapidjson::Document documentEoS;
	documentEoS.Parse(END_STREAM_MESSAGE.c_str());

	rapidjson::Value& series = documentEoS[SERIES_KEY.c_str()];
	series.SetInt(currentSeries);

	rapidjson::Value keyAcquisitionID(Eiger::ACQUISITION_ID_KEY.c_str(), documentEoS.GetAllocator());
	rapidjson::Value valueAcquisitionID(currentAcquisitionID, documentEoS.GetAllocator());
	documentEoS.AddMember(keyAcquisitionID, valueAcquisitionID, documentEoS.GetAllocator());

	rapidjson::StringBuffer buffer1;
	rapidjson::Writer<rapidjson::StringBuffer> writer1(buffer1);
	documentEoS.Accept(writer1);

	std::string eosMessage = buffer1.GetString();

	zmq::message_t message(eosMessage.size());
	memcpy (message.data (), eosMessage.c_str(), eosMessage.size());
	SendMessageToAllConsumers(message);
	state = DSTR_END;
	LOG4CXX_DEBUG(log, "Finished Sending Fabricated EndOfSeries Message");
}

void EigerFan::SetNumberOfConsumers(int number) {
	config.num_consumers = number;
}

std::string EigerFan::AddAcquisitionIDToPart1FromAppendix(zmq::message_t& messageAppendix) {
	std::string appendixjson(static_cast<char*>(messageAppendix.data()), messageAppendix.size());
	rapidjson::Document appendixDocument;
	appendixDocument.Parse(appendixjson.c_str());
	std::string acquisitionID;

	// Parse acquisitionID from appendix
	if (appendixDocument.HasParseError()) {
		LOG4CXX_ERROR(log, "Error parsing Global Header Appendix message into json");
	} else {
		if (appendixDocument.HasMember(Eiger::ACQUISITION_ID_KEY.c_str()) == true) {
			rapidjson::Value& acquisitionIDValue = appendixDocument[ACQUISITION_ID_KEY.c_str()];
			acquisitionID = acquisitionIDValue.GetString();
			currentAcquisitionID = acquisitionID;
		} else {
			LOG4CXX_WARN(log, "No acquisition ID in global header appendix");
		}
	}
	return AddAcquisitionIDToPart1(acquisitionID);
}

std::string EigerFan::AddAcquisitionIDToPart1(std::string acquisitionID) {

	rapidjson::Value keyAcquisitionID(Eiger::ACQUISITION_ID_KEY.c_str(), jsonDocument.GetAllocator());
	rapidjson::Value valueAcquisitionID(acquisitionID, jsonDocument.GetAllocator());
	jsonDocument.AddMember(keyAcquisitionID, valueAcquisitionID, jsonDocument.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	jsonDocument.Accept(writer);
	return buffer.GetString();
}
