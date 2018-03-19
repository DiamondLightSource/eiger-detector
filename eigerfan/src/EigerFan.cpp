//
// Created by up45 on 23/03/17.
//
#include <iostream>
#include <string>
#include <sstream>
#include "EigerFan.h"
#include "boost/date_time/posix_time/posix_time.hpp"

// Utility variables
int more;
size_t more_size = sizeof (more);

using namespace Eiger;

/**
 * Get a user-friendly string from a state enum value
 *
 * \param[in] state State to get the string for
 * \return The specified state in string format
 */
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

/**
 * Default constructor for the EigerFan class
 */
EigerFan::EigerFan()
: ctx_(EigerFanDefaults::DEFAULT_NUM_THREADS),
  controlSocket(ctx_, ZMQ_ROUTER) {
	this->log = log4cxx::Logger::getLogger("ED.EigerFan");
	LOG4CXX_INFO(log, "Creating EigerFan object from default options");
	killRequested = false;
	state = WAITING_CONSUMERS;
	currentSeries = 0;
	currentConsumerIndexToSendTo = 0;
	lastFrameSent = 0;
	configuredOffset = 0;
	currentOffset = 0;
}

/**
 * Constructor taking config options
 *
 * \param[in] config_ Config options
 */
EigerFan::EigerFan(EigerFanConfig config_)
: ctx_(config_.num_zmq_threads),
  controlSocket(ctx_, ZMQ_ROUTER) {
	this->log = log4cxx::Logger::getLogger("ED.EigerFan");
	config = config_;
	LOG4CXX_INFO(log, "Creating EigerFan object from config options");
	killRequested = false;
	state = WAITING_CONSUMERS;
	currentSeries = 0;
	currentConsumerIndexToSendTo = 0;
	lastFrameSent = 0;
	configuredOffset = 0;
	currentOffset = 0;
}

/**
 * Destructor
 */
EigerFan::~EigerFan() {
}

/**
 * Main method to run the EigerFan
 *
 * Will loop until kill is requested
 */
void EigerFan::run() {
  LOG4CXX_INFO(log, "EigerFan::run()");
	LOG4CXX_INFO(log, "Starting EigerFan");
	int linger = 100; // Socket linger timeout in milliseconds

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

		boost::shared_ptr<zmq::socket_t> sendSocket(new zmq::socket_t(ctx_, ZMQ_PUSH));
		sendSocket->setsockopt (ZMQ_SNDHWM, &SEND_HWM, sizeof (SEND_HWM));
		sendSocket->bind(fanAddress.str().c_str());
		sendSocket->setsockopt (ZMQ_LINGER, &linger, sizeof (linger));
		EigerConsumer consumer;
		consumer.connected = false;
		consumer.sendSocket = sendSocket;
		consumers.push_back(consumer);
	}

	std::vector<boost::shared_ptr<zmq::socket_t> > monitorSockets;
	for (int i = 0; i < config.num_consumers; i++) {
		std::ostringstream monitorAddress;
		int port = config.fan_channel_port_start + i;
		monitorAddress << "inproc://monitor-sender" << port;

		// Setup monitor for Fan Send Socket to listen to accepted and disconnected events

		zmq::socket_t* socketToMonitor = consumers[i].sendSocket.get();

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

	while (ExpectedConsumersConnected() != true && killRequested != true) {
		zmq::message_t pollMessage;
		zmq::poll (&preRunPollItems [0], config.num_consumers + 1, -1);

		if (preRunPollItems [0].revents & ZMQ_POLLIN) {
			zmq::message_t idMessage;
			controlSocket.recv(&idMessage);
			controlSocket.recv(&pollMessage);
			HandleControlMessage(pollMessage, idMessage);
		}

		for (int i = 0; i < config.num_consumers; i++) {
			if (preRunPollItems [i+1].revents & ZMQ_POLLIN) {
				monitorSockets[i]->recv(&pollMessage);
				HandleMonitorMessage(pollMessage, monitorSockets[i], i);
			}
		}
	}

	if (killRequested) {
		LOG4CXX_INFO(log, "Kill was requested before all consumers had joined");
		for (int i = 0; i < config.num_consumers; i++) {
			monitorSockets[i]->close();
			consumers[i].sendSocket->close();
		}
		controlSocket.close();
		return;
	}

	std::ostringstream oss;
	oss << "All " << GetNumberOfConnectedConsumers() << " expected consumers connected. Connecting to Eiger Stream";

	LOG4CXX_INFO(log, oss.str());

	std::string streamConnectionAddress("tcp://");
	streamConnectionAddress.append(config.eiger_channel_address);
	streamConnectionAddress.append(":");
	streamConnectionAddress.append(config.eiger_channel_port);
	LOG4CXX_INFO(log, std::string("Connecting to stream address at ").append(streamConnectionAddress));

	//  Initialise run poll set
	zmq::pollitem_t runPollItems [config.num_consumers + 1 + config.num_zmq_sockets];
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

	std::vector<boost::shared_ptr<zmq::socket_t> > streamSockets;
	int streamSocketPollItemStartIndex = config.num_consumers + 1;
	for (int i = 0; i < config.num_zmq_sockets; i++) {

		boost::shared_ptr<zmq::socket_t> recvSocket(new zmq::socket_t(ctx_, ZMQ_PULL));
		recvSocket->setsockopt (ZMQ_RCVHWM, &RECEIVE_HWM, sizeof (RECEIVE_HWM));
		recvSocket->connect(streamConnectionAddress.c_str());
		recvSocket->setsockopt (ZMQ_LINGER, &linger, sizeof (linger));

		streamSockets.push_back(recvSocket);

		zmq_pollitem_t recvRunPollItem;
		zmq::socket_t* streamSocket = recvSocket.get();
		recvRunPollItem.socket = *streamSocket;
		recvRunPollItem.fd = 0;
		recvRunPollItem.events = ZMQ_POLLIN;
		recvRunPollItem.revents = 0;
		runPollItems[streamSocketPollItemStartIndex + i] = recvRunPollItem;
	}

	state = WAITING_STREAM;

	//  Process tasks forever or until kill is requested
	while (killRequested != true) {
		zmq::message_t message;
		zmq::poll (&runPollItems [0], config.num_consumers + 1 + config.num_zmq_sockets, -1);

		// Control socket events
		if (runPollItems [0].revents & ZMQ_POLLIN) {
			zmq::message_t idMessage;
			controlSocket.recv(&idMessage);
			controlSocket.recv(&message);
			HandleControlMessage(message, idMessage);
			if (killRequested) {
				break;
			}
		}

		// Monitor socket events
		for (int i = 0; i < config.num_consumers; i++) {
			if (runPollItems [i+1].revents & ZMQ_POLLIN) {
				monitorSockets[i]->recv(&message);
				HandleMonitorMessage(message, monitorSockets[i], i);
			}
		}

		// Stream socket events
		for (int i = 0; i < config.num_zmq_sockets; i++) {
			if (runPollItems [streamSocketPollItemStartIndex + i].revents & ZMQ_POLLIN) {
                streamSockets[i]->recv(&message);
				HandleStreamMessage(message, streamSockets[i]);
			}
		}
	}

	LOG4CXX_INFO(log, "Shutting down EigerFan sockets");
	for (int i = 0; i < config.num_consumers; i++) {
		monitorSockets[i]->close();
		consumers[i].sendSocket->close();
	}

	for (int i = 0; i < config.num_zmq_sockets; i++) {
		streamSockets[i]->close();
	}

	controlSocket.close();
}

/**
 * Request the EigerFan to stop
 */
void EigerFan::Stop() {
	LOG4CXX_INFO(log, "Stop requested");
	killRequested = true;
	state = KILL_REQUESTED;
}

/**
 * Handle a message from the zmq stream
 *
 * \param[in] message The zeromq message to handle
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleStreamMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket) {

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
				// If on the first frame, set the current offset to any configured offset
				if (frame == 0) {
					currentOffset = configuredOffset % config.num_consumers;
					configuredOffset = 0;
					lastFrameSent = 0;
				}
                currentConsumerIndexToSendTo = ((frame + currentOffset) / config.block_size) % config.num_consumers;
				HandleImageDataMessage(message, socket);
				if (frame > lastFrameSent) {
					lastFrameSent = frame;
				}
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
	socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		socket->recv(&messagePartExtra);
		LOG4CXX_ERROR(log, "Unexpected unhandled message in stream");
		socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
	}
}

/**
 * Handle the Global Header message
 *
 * This is a multipart message sent by the Eiger at the start of an acquisition and can contain different amounts of meta data
 *
 * \param[in] messagePart1 The first part of the message
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleGlobalHeaderMessage(zmq::message_t &messagePart1, boost::shared_ptr<zmq::socket_t> socket) {
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
			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				LOG4CXX_DEBUG(log, "Header has appendix");
				zmq::message_t messageAppendix;
				socket->recv(&messageAppendix);
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
			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 1 part but expected 2 for 'basic' detail");
				return;
			}

			zmq::message_t messagePart2;
			socket->recv(&messagePart2);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				LOG4CXX_DEBUG(log, "Header has appendix");
				zmq::message_t messageAppendix;
				socket->recv(&messageAppendix);
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
			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 1 part but expected 8 for 'all' detail");
				return;
			}

			// Part 2
			zmq::message_t messagePart2;
			socket->recv(&messagePart2);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 2 part but expected 8 for 'all' detail");
				return;
			}

			// Part 3
			zmq::message_t messagePart3;
			socket->recv(&messagePart3);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 3 part but expected 8 for 'all' detail");
				return;
			}

			// Part 4
			zmq::message_t messagePart4;
			socket->recv(&messagePart4);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 4 part but expected 8 for 'all' detail");
				return;
			}

			// Part 5
			zmq::message_t messagePart5;
			socket->recv(&messagePart5);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 5 part but expected 8 for 'all' detail");
				return;
			}

			// Part 6
			zmq::message_t messagePart6;
			socket->recv(&messagePart6);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 6 part but expected 8 for 'all' detail");
				return;
			}

			// Part 7
			zmq::message_t messagePart7;
			socket->recv(&messagePart7);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more != MORE_MESSAGES) {
				LOG4CXX_ERROR(log, "Header only contained 7 part but expected 8 for 'all' detail");
				return;
			}

			// Part 8
			zmq::message_t messagePart8;
			socket->recv(&messagePart8);

			socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (more == MORE_MESSAGES) {
				LOG4CXX_DEBUG(log, "Header has appendix");
				zmq::message_t messageAppendix;
				socket->recv(&messageAppendix);
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

/**
 * Handle the Image Data message
 *
 * This is a a multipart message sent by the Eiger containing the image and associated meta data
 *
 * \param[in] messagePart1 The first part of the message
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleImageDataMessage(zmq::message_t &messagePart1, boost::shared_ptr<zmq::socket_t> socket) {
	LOG4CXX_DEBUG(log, "Handling Image Data Message");

	socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
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
	socket->recv(&messagePart2);

	socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		LOG4CXX_ERROR(log, "Image Data only contained 2 parts");
		return;
	}

	// Part 3 - data blob
	zmq::message_t messagePart3;
	socket->recv(&messagePart3);

	socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more != MORE_MESSAGES) {
		LOG4CXX_ERROR(log, "Image Data only contained 3 parts");
		return;
	}

	//Part 4 - times
	zmq::message_t messagePart4;
	socket->recv(&messagePart4);

	// Handle appendix
	socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
	if (more == MORE_MESSAGES) {
		LOG4CXX_DEBUG(log, "Image has appendix");
		zmq::message_t messageAppendix;
		socket->recv(&messageAppendix);

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
		LOG4CXX_WARN(log, std::string("Received Image Data message in unexpected state: ").append(GetStateString(state)));
	}
	state = DSTR_IMAGE;
	LOG4CXX_DEBUG(log, "Finished Handling Image Data Message");
}

/**
 * Handle the Image Data message
 *
 * This is a single message sent by the Eiger at the end of an acquisition
 *
 * \param[in] message The zeromq message
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleEndOfSeriesMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket) {
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

/**
 * Handle messages from the monitoring of the zeromq connections
 *
 * Used to detect when consumers connect and disconnect from the EigerFan
 *
 * \param[in] message The zeromq message
 * \param[in] socket The socket that the message was received on
 * \param[in] rank The rank of the consumer connecting or disconnecting
 */
void EigerFan::HandleMonitorMessage(zmq::message_t &message, boost::shared_ptr<zmq::socket_t> socket, int rank) {
	LOG4CXX_DEBUG(log, "Handling Monitor Message");

	// Get the event code from the message, which is a number contained in the first 16 bits
	uint16_t event = *(uint16_t *) (message.data());

	if (event == ZMQ_EVENT_ACCEPTED) {
		std::ostringstream logMessage;
		logMessage << "Consumer connected (rank: " << rank << ")";
		LOG4CXX_INFO(log, logMessage.str());
		consumers[rank].connected++;
		if (consumers[rank].connected > 1) {
			LOG4CXX_ERROR(log, "More than one consumer connected (" << consumers[rank].connected << ") with the same rank (" << rank << ")");
		}
		if (WAITING_CONSUMERS != state) {
			LOG4CXX_ERROR(log, "Consumer connected whilst in state: " << GetStateString(state));
		}
	} else if (event == ZMQ_EVENT_DISCONNECTED) {
		std::ostringstream logMessage;
		logMessage << "Consumer disconnected (rank: " << rank << ")";
		LOG4CXX_WARN(log, logMessage.str());
		consumers[rank].connected--;
		if (consumers[rank].connected < 0) {
			LOG4CXX_ERROR(log, "Number of consumers connected for rank " << rank << " was less than 0");
			consumers[rank].connected = 0;
		}
		if (WAITING_CONSUMERS != state) {
			LOG4CXX_ERROR(log, "Consumer disconnected whilst in state: " << GetStateString(state));
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

/**
 * Handle control messages
 *
 * \param[in] message The zeromq message containing the control command
 * \param[in] idMessage The zeromq id message to identify the sender
 */
void EigerFan::HandleControlMessage(zmq::message_t &message, zmq::message_t &idMessage) {

	std::string jsonCommand(static_cast<char*>(message.data()), message.size());

	rapidjson::Document ctrlDocument;

	std::string replyString(CONTROL_RESPONSE_UNABLE);

	ctrlDocument.Parse(jsonCommand.c_str());
	if (ctrlDocument.HasParseError() || !ctrlDocument.HasMember(CONTROL_CMD_KEY.c_str())) {
		LOG4CXX_ERROR(log, "Error parsing control message into json: " << jsonCommand);
	} else {
		rapidjson::Value& cmdValue = ctrlDocument[CONTROL_CMD_KEY.c_str()];
		std::string command(cmdValue.GetString());

		if (command.compare(CONTROL_STATUS) == 0) {

			rapidjson::Document document;
			document.SetObject();

			// Add State
			rapidjson::Value keyState("state", document.GetAllocator());
			rapidjson::Value valueState(GetStateString(state), document.GetAllocator());
			document.AddMember(keyState, valueState, document.GetAllocator());

			// Add Number of connected
			rapidjson::Value keyNumConn("num_conn", document.GetAllocator());
			rapidjson::Value valueNumConn(GetNumberOfConnectedConsumers());
			document.AddMember(keyNumConn, valueNumConn, document.GetAllocator());

			// Add Current series
			rapidjson::Value keySeries("series", document.GetAllocator());
			rapidjson::Value valueSeries(currentSeries);
			document.AddMember(keySeries, valueSeries, document.GetAllocator());

			// Add Last Frame sent
			rapidjson::Value keyFrame("frame", document.GetAllocator());
			rapidjson::Value valueFrame(lastFrameSent);
			document.AddMember(keyFrame, valueFrame, document.GetAllocator());

			// Add current offset being applied to the fan distribution
			rapidjson::Value keyOffset("fan_offset", document.GetAllocator());
			rapidjson::Value valueOffset(currentOffset);
			document.AddMember(keyOffset, valueOffset, document.GetAllocator());

			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

			document.Accept(writer);

			std::ostringstream oss;
			oss << "{\"msg_type\":\"ack\",\"msg_val\":\"status\", \"params\": " << buffer.GetString() << "}";
			replyString.assign(oss.str());

		} else if (command.compare(CONTROL_CONFIGURE) == 0) {
			LOG4CXX_DEBUG(log, std::string("Handling Control Configure Message: ").append(jsonCommand));

			if (ctrlDocument.HasMember(CONTROL_PARAM_KEY.c_str())) {
				rapidjson::Value& paramsValue = ctrlDocument[CONTROL_PARAM_KEY.c_str()];
				if (paramsValue.HasMember(CONTROL_KILL.c_str())) {
					Stop();
					replyString.assign(CONTROL_RESPONSE_OK.c_str());
				} else if (paramsValue.HasMember(CONTROL_CLOSE.c_str())) {
					// Close gracefully - if currently acquiring data, send end of stream message and terminate
					if (state == DSTR_HEADER || state == DSTR_IMAGE) {
						SendFabricatedEndMessage();
					}
					Stop();
					replyString.assign(CONTROL_RESPONSE_OK.c_str());
				} else if (paramsValue.HasMember(CONTROL_OFFSET.c_str())) {
					// Change the consumer to send to based on an offset value
					configuredOffset = paramsValue[CONTROL_OFFSET.c_str()].GetInt();
					LOG4CXX_INFO(log, "Offset changed to " << configuredOffset);
					replyString.assign(CONTROL_RESPONSE_OK.c_str());
				} else {
					LOG4CXX_ERROR(log, "No recognised configure parameter");
					replyString.assign(CONTROL_RESPONSE_NOCFGPARAM);
				}
			} else {
				LOG4CXX_ERROR(log, "No parameter on configure command");
				replyString.assign(CONTROL_RESPONSE_NOPARAM.c_str());
			}
		} else {
			LOG4CXX_WARN(log, "Received unknown control command: " << command);
		}
	}

	rapidjson::Document statusDocument;
	statusDocument.Parse(replyString.c_str());

	std::string ts(boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::local_time()));
	rapidjson::Value keyTimestamp("timestamp", statusDocument.GetAllocator());
	rapidjson::Value valueTimestamp(ts, statusDocument.GetAllocator());
	statusDocument.AddMember(keyTimestamp, valueTimestamp, statusDocument.GetAllocator());

  rapidjson::Value& idValue = ctrlDocument[CONTROL_ID_KEY.c_str()];
  unsigned int msg_id(idValue.GetInt());
  rapidjson::Value keyId("id", statusDocument.GetAllocator());
  rapidjson::Value valueId(msg_id);
  statusDocument.AddMember(keyId, valueId, statusDocument.GetAllocator());

	rapidjson::StringBuffer statusBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> statusWriter(statusBuffer);
	statusDocument.Accept(statusWriter);

	std::string replyWithTimestamp = statusBuffer.GetString();

	controlSocket.send(idMessage, ZMQ_SNDMORE);

	zmq::message_t reply (replyWithTimestamp.size());
	memcpy (reply.data(), replyWithTimestamp.c_str(), replyWithTimestamp.size());
	controlSocket.send(reply);

	// Handle any message parts at the end
	controlSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	while (more == MORE_MESSAGES) {
		zmq::message_t messagePartExtra;
		controlSocket.recv(&messagePartExtra);
		LOG4CXX_ERROR(log, "Control contained more parts than expected");
	}
}

/**
 * Send a message to all consumers
 *
 * \param[in] message The zeromq message to send
 * \param[in] flags Any flags to apply to the message (e.g. more messages to come)
 */
void EigerFan::SendMessageToAllConsumers(zmq::message_t& message, int flags) {
	int numConsumersToSendTo = config.num_consumers;
	LOG4CXX_DEBUG(log, "Sending message to all consumers. Number of consumers = " << GetNumberOfConnectedConsumers());
	// Make as many copies as necessary (number to send minus 1) and send them
	for (int i = 0; i < numConsumersToSendTo-1; i++) {
		if (consumers.at(i).connected > 0) {
			zmq::message_t messageCopy;
			messageCopy.copy(&message);
			if (consumers.at(i).sendSocket->send(messageCopy, flags) == false) {
				LOG4CXX_ERROR(log, "Send socket returned false");
			}
		} else {
			LOG4CXX_ERROR(log, "Consumer with rank " << i << " not connected");
		}
	}
	// Send the actual message for the last one
	if (consumers.at(numConsumersToSendTo-1).connected > 0) {
		if (consumers.at(numConsumersToSendTo-1).sendSocket->send(message, flags) == false) {
			LOG4CXX_ERROR(log, "Send socket returned false");
		}
	} else {
		LOG4CXX_ERROR(log, "Consumer with rank " << numConsumersToSendTo-1 << " not connected");
	}

	LOG4CXX_DEBUG(log, "Finished Sending message to all consumers");
}

/**
 * Send a list of messages to all consumers
 *
 * \param[in] messageList The list of zeromq messages to send
 */
void EigerFan::SendMessagesToAllConsumers(std::vector<zmq::message_t*> &messageList) {
	int messageListSize = messageList.size();
	int numConsumersToSendTo = config.num_consumers;
	LOG4CXX_DEBUG(log, "Sending multiple messages to all consumers. Number of consumers = " << GetNumberOfConnectedConsumers());
	// Make as many copies as necessary (number to send minus 1) and send them
	for (int consumerCount = 0; consumerCount < numConsumersToSendTo-1; consumerCount++) {
		if (consumers.at(consumerCount).connected > 0) {
			for (int messageCount = 0; messageCount < messageListSize; messageCount++)
			{
				zmq::message_t messageCopy;
				messageCopy.copy(messageList[messageCount]);
				if (messageCount != messageListSize - 1) {
					if (consumers.at(consumerCount).sendSocket->send(messageCopy, ZMQ_SNDMORE) == false) {
						LOG4CXX_ERROR(log, "Send socket returned false");
					}
				} else {
					if (consumers.at(consumerCount).sendSocket->send(messageCopy) == false) {
						LOG4CXX_ERROR(log, "Send socket returned false");
					}
				}
			}
		} else {
			LOG4CXX_ERROR(log, "Consumer with rank " << consumerCount << " not connected");
		}
	}
	// Send the actual message for the last one
	if (consumers.at(numConsumersToSendTo-1).connected > 0) {
		for (int messageCount = 0; messageCount < messageListSize; messageCount++)
		{
			if (messageCount != messageListSize - 1) {
				if (consumers.at(numConsumersToSendTo-1).sendSocket->send(*messageList[messageCount], ZMQ_SNDMORE) == false) {
					LOG4CXX_ERROR(log, "Send socket returned false");
				}
			} else {
				if (consumers.at(numConsumersToSendTo-1).sendSocket->send(*messageList[messageCount]) == false) {
					LOG4CXX_ERROR(log, "Send socket returned false");
				}
			}
		}
	} else {
		LOG4CXX_ERROR(log, "Consumer with rank " << numConsumersToSendTo-1 << " not connected");
	}

	messageList.clear();
	LOG4CXX_DEBUG(log, "Finished Sending multiple messages to all consumers");
}

/**
 * Send a single messages to the appropriate consumer
 *
 * Relies on the currentConsumerIndexToSendTo variable being set to determine which consumer to send to
 *
 * \param[in] message The zeromq message to send
 * \param[in] flags Any flags to apply to the message (e.g. more messages to come)
 */
void EigerFan::SendMessageToSingleConsumer(zmq::message_t& message, int flags) {
	LOG4CXX_DEBUG(log, "Sending message to single consumers at index:" << currentConsumerIndexToSendTo);

	// Send the message to a consumer
	if (consumers.at(currentConsumerIndexToSendTo).connected > 0) {
		if (consumers.at(currentConsumerIndexToSendTo).sendSocket->send(message, flags) == false) {
			LOG4CXX_ERROR(log, "Send socket returned false");
		}
	} else {
		LOG4CXX_ERROR(log, "Consumer with rank " << currentConsumerIndexToSendTo << " not connected");
	}

	LOG4CXX_DEBUG(log, "Finished Sending message to single consumer");
}

/**
 * Send a fabricated end message
 *
 * This is needed when the EigerFan is commanded to shut down before finishing an acquisition.
 * Sending a fabricated end message means that any downstream processors can close cleanly
 */
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

/**
 * Sets the configure number of consumers
 *
 * \param[in] number The number of consumers
 */
void EigerFan::SetNumberOfConsumers(int number) {
	config.num_consumers = number;
}

/**
 * Parses the acquisition id from the zeromq message containing the appendix and
 * adds it to the first message part.
 *
 * This is to enable easier downstream processing
 *
 * \param[in] messageAppendix The message containing the appendix with the acquisition id in
 * \return The string of the first message part which now also contains the acquisition id
 */
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
			LOG4CXX_INFO(log, "Acquisition ID is [" << currentAcquisitionID << "]");
		} else {
			LOG4CXX_WARN(log, "No acquisition ID in global header appendix");
		}
	}
	return AddAcquisitionIDToPart1(acquisitionID);
}

/**
 * Adds the specified acquisiton ID to the first message part.
 *
 * This is to enable easier downstream processing.
 * This relies on the class variable jsonDocument still containing the
 * first message part.
 *
 * \param[in] acquisitionID The acquisition id
 * \return The string of the first message part which now also contains the acquisition id
 */
std::string EigerFan::AddAcquisitionIDToPart1(std::string acquisitionID) {
	rapidjson::Value keyAcquisitionID(Eiger::ACQUISITION_ID_KEY.c_str(), jsonDocument.GetAllocator());
	rapidjson::Value valueAcquisitionID(acquisitionID, jsonDocument.GetAllocator());
	jsonDocument.AddMember(keyAcquisitionID, valueAcquisitionID, jsonDocument.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	jsonDocument.Accept(writer);
	return buffer.GetString();
}

/**
 * Gets the number of currently connected consumers
 *
 * \return The number of currently connected consumers
 */
int EigerFan::GetNumberOfConnectedConsumers() {
	int numConnected = 0;
	for (int i = 0; i < config.num_consumers; i++) {
		if (consumers[i].connected > 0) {
			numConnected++;
			if (consumers[i].connected > 1) {
				LOG4CXX_ERROR(log, "More than one consumer connected (" << consumers[i].connected << ") with the same rank (" << i << ")");
			}
		}
	}
	return numConnected;
}

/**
 * Gets whether the expected number of consumers are connected
 *
 * Will return false if more than one consumer is connected to a single channel
 *
 * \return True if all expected consumers are connected
 */
bool EigerFan::ExpectedConsumersConnected() {
	for (int i = 0; i < config.num_consumers; i++) {
		if (consumers[i].connected != 1) {
			return false;
		}
	}
	return true;
}
