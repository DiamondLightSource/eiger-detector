/*
 * EigerProcessPlugin.cpp
 *
 *  Created on: 23 March 2017
 *      Author: Ulrik Pedersen
 */

#include <fcntl.h> // open, O_CREAT, O_RDWR
#include <iomanip> // std::setw, std::setfill
#include <iostream>
#include <string>
#include <sstream>

#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/filesystem.hpp>

#include "EigerFan.h"

// Utility variables
int more;
size_t more_size = sizeof (more);

using namespace Eiger;


/** Log an error with the given message and the current errno
 *
 * @param message Message to append errno onto
 *
*/
#define LOG_WITH_ERRNO(log, message) { \
  std::stringstream error; \
  error << message << " [errno: " << errno << " - " << strerror(errno) << "]"; \
  LOG4CXX_ERROR(log, error.str()); \
}

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

static std::string BROKER_INPROC_ENDPOINT = "inproc://broker";

/**
 * Create a string of value padded with zeroes.
 *
 * \param[in] value Value to pad
 * \return The padded string
 *
*/
std::string PadInt(int value) {
  std::ostringstream ss;
  ss << std::setw(6) << std::setfill('0') << value;
  return ss.str();
}

/**
 * Default constructor for the EigerFan class
 */
EigerFan::EigerFan()
: ctx_(EigerFanDefaults::DEFAULT_NUM_CONTEXT_THREADS),
  controlSocket(ctx_, ZMQ_ROUTER),
  forwardSocket(ctx_, ZMQ_PUSH),
  broker(BROKER_INPROC_ENDPOINT, EigerFanDefaults::DEFAULT_NUM_THREADS)
{
  this->log = log4cxx::Logger::getLogger("ED.EigerFan");
  LOG4CXX_INFO(log, "Creating EigerFan object from default options");
  killRequested = false;
  state = WAITING_CONSUMERS;
  currentSeries = 0;
  currentConsumerIndexToSendTo = 0;
  lastFrameSent = 0;
  num_frames_sent = 0;
  configuredOffset = 0;
  currentOffset = 0;
  numConnectedForwardingSockets = 0;
  forwardStream = false;
  devShmCache = false;
}

/**
 * Constructor taking config options
 *
 * \param[in] config_ Config options
 */
EigerFan::EigerFan(EigerFanConfig config_)
: ctx_(config_.num_zmq_context_threads),
  controlSocket(ctx_, ZMQ_ROUTER),
  forwardSocket(ctx_, ZMQ_PUSH),
  broker(BROKER_INPROC_ENDPOINT, config_.num_threads)
{
  this->log = log4cxx::Logger::getLogger("ED.EigerFan");
  config = config_;
  LOG4CXX_INFO(log, "Creating EigerFan object from config options");
  killRequested = false;
  state = WAITING_CONSUMERS;
  currentSeries = 0;
  currentConsumerIndexToSendTo = 0;
  lastFrameSent = 0;
  num_frames_sent = 0;
  configuredOffset = 0;
  currentOffset = 0;
  numConnectedForwardingSockets = 0;
  forwardStream = false;
  devShmCache = false;
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

  // Setup Control socket
  std::string controlAddress("tcp://*:");
  controlAddress.append(config.ctrl_channel_port);
  LOG4CXX_INFO(log, std::string("Binding control address to ").append(controlAddress));
  controlSocket.bind (controlAddress.c_str());
  controlSocket.setsockopt (ZMQ_LINGER, &LINGER_TIMEOUT, sizeof (LINGER_TIMEOUT));

  // Setup Fan Send Sockets
  for (int i = 0; i < config.num_consumers; i++) {
    std::ostringstream fanAddress;
    int port = config.fan_channel_port_start + i;
    fanAddress << "tcp://*:" << port;

    LOG4CXX_INFO(log, std::string("Binding fan send address to ").append(fanAddress.str()));

    boost::shared_ptr<zmq::socket_t> sendSocket(new zmq::socket_t(ctx_, ZMQ_PUSH));
    sendSocket->setsockopt(ZMQ_SNDHWM, &SEND_HWM, sizeof (SEND_HWM));
    sendSocket->bind(fanAddress.str().c_str());
    sendSocket->setsockopt (ZMQ_LINGER, &LINGER_TIMEOUT, sizeof (LINGER_TIMEOUT));
    EigerConsumer consumer;
    consumer.connected = false;
    consumer.sendSocket = sendSocket;
    consumers.push_back(consumer);
    num_frames_consumed.push_back(0);
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
    monitorSocket->setsockopt (ZMQ_LINGER, &LINGER_TIMEOUT, sizeof (LINGER_TIMEOUT));
    monitorSockets.push_back(monitorSocket);
  }

  // Setup Forwarding Socket
  std::string forwardAddress("tcp://*:");
  forwardAddress.append(config.forward_channel_port);
  LOG4CXX_INFO(log, std::string("Binding forwarding address to ").append(forwardAddress));
  forwardSocket.setsockopt(ZMQ_SNDHWM, &SEND_HWM, sizeof (SEND_HWM));
  forwardSocket.bind(forwardAddress.c_str());
  forwardSocket.setsockopt (ZMQ_LINGER, &LINGER_TIMEOUT, sizeof (LINGER_TIMEOUT));

  // Setup Forwarding Socket monitor
  std::ostringstream forwardMonitorAddress;
  forwardMonitorAddress << "inproc://forward-monitor-sender";

  int rc = zmq_socket_monitor(forwardSocket, forwardMonitorAddress.str().c_str(), ZMQ_EVENT_ACCEPTED | ZMQ_EVENT_DISCONNECTED);
  if (rc < 0) {
    LOG4CXX_ERROR(log, "Error setting up forwarding monitor. 0MQ Error number: " << zmq_errno());
    return;
  }
  zmq::socket_t forwardMonitorSocket(ctx_, ZMQ_PAIR);
  forwardMonitorSocket.connect(forwardMonitorAddress.str().c_str());
  forwardMonitorSocket.setsockopt (ZMQ_LINGER, &LINGER_TIMEOUT, sizeof (LINGER_TIMEOUT));

  // Wait for configured number of consumers to connect
  LOG4CXX_INFO(log, "Waiting for Consumers");

  //  Initialise pre-run poll set (num consumers + control socket + forward socket)
  zmq::pollitem_t preRunPollItems [config.num_consumers + 1 + 1];
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

  zmq_pollitem_t forwardMonitorPollItem;
  forwardMonitorPollItem.socket = forwardMonitorSocket;
  forwardMonitorPollItem.fd = 0;
  forwardMonitorPollItem.events = ZMQ_POLLIN;
  forwardMonitorPollItem.revents = 0;
  preRunPollItems[config.num_consumers + 1] = forwardMonitorPollItem;

  while (ExpectedConsumersConnected() != true && killRequested != true) {
    zmq::message_t pollMessage;
    zmq::poll (&preRunPollItems [0], config.num_consumers + 1 + 1, -1);

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

    if (preRunPollItems [config.num_consumers + 1].revents & ZMQ_POLLIN) {
      forwardMonitorSocket.recv(&pollMessage);
      HandleForwardMonitorMessage(pollMessage, forwardMonitorSocket);
    }
  }

  if (killRequested) {
    LOG4CXX_INFO(log, "Kill was requested before all consumers had joined");
    for (int i = 0; i < config.num_consumers; i++) {
      monitorSockets[i]->close();
      consumers[i].sendSocket->close();
    }
    forwardSocket.close();
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
  zmq::pollitem_t runPollItems [config.num_consumers + 1 + 1]; // consumers + control + forward
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

  zmq_pollitem_t forwardinMonitorPollItem;
  forwardinMonitorPollItem.socket = forwardMonitorSocket;
  forwardinMonitorPollItem.fd = 0;
  forwardinMonitorPollItem.events = ZMQ_POLLIN;
  forwardinMonitorPollItem.revents = 0;
  runPollItems[config.num_consumers + 1] = forwardinMonitorPollItem;

  // Spawn rx thread
  LOG4CXX_INFO(log, "Spawning rx thread");
  this->rx_thread_ = boost::shared_ptr<boost::thread>(
    new boost::thread(boost::bind(&EigerFan::HandleRxSocket, this, streamConnectionAddress, config.num_zmq_context_threads))
  );

  while (state != WAITING_STREAM) {
    sleep(1);
  }

  //  Process tasks forever or until kill is requested
  LOG4CXX_INFO(log, "Processing control tasks");
  while (killRequested != true) {
    zmq::message_t message;
    zmq::poll(&runPollItems [0], config.num_consumers + 1 + 1, -1);  // Monitor per consumer + control + forward

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

    // Forwarding Monitor socket events
    if (runPollItems [config.num_consumers + 1].revents & ZMQ_POLLIN) {
      forwardMonitorSocket.recv(&message);
      HandleForwardMonitorMessage(message, forwardMonitorSocket);
    }
  }

  LOG4CXX_INFO(log, "Shutting down EigerFan sockets");
  for (int i = 0; i < config.num_consumers; i++) {
    monitorSockets[i]->close();
    consumers[i].sendSocket->close();
  }

  forwardSocket.close();
  controlSocket.close();
}

/**
 * Connect broker to detector and handle the messages it produces
 */
void EigerFan::HandleRxSocket(std::string& endpoint, int num_zmq_context_threads) {
  zmq::context_t inproc_context(num_zmq_context_threads);
  zmq::socket_t rx_socket(inproc_context, ZMQ_PULL);
  rx_socket.setsockopt(ZMQ_RCVHWM, &RECEIVE_HWM, sizeof(RECEIVE_HWM));
  rx_socket.bind(BROKER_INPROC_ENDPOINT.c_str());
  rx_socket.setsockopt(ZMQ_LINGER, &LINGER_TIMEOUT, sizeof(LINGER_TIMEOUT));
  this->broker.connect(endpoint, &inproc_context);

  state = WAITING_STREAM;
  LOG4CXX_INFO(log, "Processing rx socket");

  zmq::message_t message;
  zmq::pollitem_t pollItems [] = {{rx_socket, 0, ZMQ_POLLIN, 0}};
  boost::shared_ptr<zmq::socket_t> socket_ptr(&rx_socket);
  while (!killRequested) {
    // Stream socket events
    zmq::poll(&pollItems[0], 1, -1);
    if (pollItems[0].revents & ZMQ_POLLIN) {
      rx_socket.recv(&message);
      HandleStreamMessage(message, socket_ptr);
    }
  }

  rx_socket.close();
  broker.shutdown();

  LOG4CXX_INFO(log, "RX thread done");
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

    // Interpret the message as a JSON string and store in the global json document
    jsonDocument.Parse(smessage.c_str());
    if (jsonDocument.HasParseError()) {
      LOG4CXX_ERROR(log, "Error parsing stream message into json");
    } else {
      rapidjson::Value& headerTypeValue = jsonDocument[HEADER_TYPE_KEY.c_str()];
      std::string htype(headerTypeValue.GetString());
      if (htype.compare(GLOBAL_HEADER_TYPE) == 0) {
        // At the start of an acquisition so set the current offset to any configured offset
        currentOffset = configuredOffset;
        configuredOffset = 0;
        lastFrameSent = 0;
        broker.start_message_counter();
        num_frames_sent = 0;
        for(int j=0; j<num_frames_consumed.size(); j++) {
          num_frames_consumed[j] = 0;
        }
        currentAcquisitionID = configuredAcquisitionID;
        // Handle Message
        HandleGlobalHeaderMessage(socket);
      } else if (htype.compare(IMAGE_HEADER_TYPE) == 0) {
        rapidjson::Value& frameValue = jsonDocument[FRAME_KEY.c_str()];
        int64_t frame(frameValue.GetInt64());
        currentConsumerIndexToSendTo = ((frame + currentOffset) / config.block_size) % config.num_consumers;
        HandleImageDataMessage(socket, frame);
        if (frame > lastFrameSent) {
          lastFrameSent = frame;
        }
        num_frames_sent++;
        if (currentConsumerIndexToSendTo < num_frames_consumed.size()) {
          num_frames_consumed[currentConsumerIndexToSendTo]++;
        }
        else {
          LOG4CXX_WARN(log, "Error counting consumer frames for logging");
        }
      } else if (htype.compare(END_HEADER_TYPE) == 0) {
        LOG4CXX_INFO(
          log,
          "End of series message received after " + boost::lexical_cast<std::string>(broker.messages_received()) + \
          " messages received and " + boost::lexical_cast<std::string>(num_frames_sent) + " frames sent."
        );
        std::string consumer_frames;
        for(int j=0; j<num_frames_consumed.size(); j++) {
          consumer_frames +=
                  boost::lexical_cast<std::string>(j) + ": " + \
                          boost::lexical_cast<std::string>(num_frames_consumed[j]) + " ";
        }
        LOG4CXX_INFO(log, "Consumer frame counts " + consumer_frames);
        HandleEndOfSeriesMessage(socket);
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
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleGlobalHeaderMessage(boost::shared_ptr<zmq::socket_t> socket) {
  std::vector<zmq::message_t*> messageList;

  // Add the Acquisition ID to part 1 for easier downstream processing
  std::string part1WithAcquisitionID = AddAcquisitionIDToPart1();
  zmq::message_t newPart1message(part1WithAcquisitionID.size());
  memcpy (newPart1message.data (), part1WithAcquisitionID.c_str(), part1WithAcquisitionID.size());

  LOG4CXX_INFO(log, std::string("Received Global Header message: ").append(part1WithAcquisitionID));

  rapidjson::Value& seriesValue = jsonDocument[SERIES_KEY.c_str()];
  currentSeries = seriesValue.GetInt();

  rapidjson::Value& headerDetailValue = jsonDocument[HEADER_DETAIL_KEY.c_str()];
  std::string headerDetail(headerDetailValue.GetString());

  this->WriteMessageToFile(newPart1message, "start_0");

  if (headerDetail.compare(HEADER_DETAIL_NONE) == 0) {
    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more == MORE_MESSAGES) {
      LOG4CXX_DEBUG(log, "Header has appendix");
      zmq::message_t messageAppendix;
      socket->recv(&messageAppendix);

      this->WriteMessageToFile(messageAppendix, "start_appendix");

      messageList.push_back(&newPart1message);
      messageList.push_back(&messageAppendix);
      SendMessagesToAllConsumers(messageList);
    } else {
      SendMessageToAllConsumers(newPart1message);
    }

  } else if (headerDetail.compare(HEADER_DETAIL_BASIC) == 0) {
    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more != MORE_MESSAGES) {
      LOG4CXX_ERROR(log, "Header only contained 1 part but expected 2 for 'basic' detail");
      return;
    }

    zmq::message_t messagePart2;
    socket->recv(&messagePart2);

    this->WriteMessageToFile(messagePart2, "start_1");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more == MORE_MESSAGES) {
      LOG4CXX_DEBUG(log, "Header has appendix");
      zmq::message_t messageAppendix;
      socket->recv(&messageAppendix);

      this->WriteMessageToFile(messageAppendix, "start_appendix");

      messageList.push_back(&newPart1message);
      messageList.push_back(&messagePart2);
      messageList.push_back(&messageAppendix);
      SendMessagesToAllConsumers(messageList);
    } else {
      messageList.push_back(&newPart1message);
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

    this->WriteMessageToFile(messagePart2, "start_1");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more != MORE_MESSAGES) {
      LOG4CXX_ERROR(log, "Header only contained 2 parts but expected 8 for 'all' detail");
      return;
    }

    // Part 3
    zmq::message_t messagePart3;
    socket->recv(&messagePart3);

    this->WriteMessageToFile(messagePart3, "start_2");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more != MORE_MESSAGES) {
      LOG4CXX_ERROR(log, "Header only contained 3 parts but expected 8 for 'all' detail");
      return;
    }

    // Part 4
    zmq::message_t messagePart4;
    socket->recv(&messagePart4);

    this->WriteMessageToFile(messagePart4, "start_3");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more != MORE_MESSAGES) {
      LOG4CXX_ERROR(log, "Header only contained 4 parts but expected 8 for 'all' detail");
      return;
    }

    // Part 5
    zmq::message_t messagePart5;
    socket->recv(&messagePart5);

    this->WriteMessageToFile(messagePart5, "start_4");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more != MORE_MESSAGES) {
      LOG4CXX_ERROR(log, "Header only contained 5 parts but expected 8 for 'all' detail");
      return;
    }

    // Part 6
    zmq::message_t messagePart6;
    socket->recv(&messagePart6);

    this->WriteMessageToFile(messagePart6, "start_5");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more != MORE_MESSAGES) {
      LOG4CXX_ERROR(log, "Header only contained 6 parts but expected 8 for 'all' detail");
      return;
    }

    // Part 7
    zmq::message_t messagePart7;
    socket->recv(&messagePart7);

    this->WriteMessageToFile(messagePart7, "start_6");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more != MORE_MESSAGES) {
      LOG4CXX_ERROR(log, "Header only contained 7 parts but expected 8 for 'all' detail");
      return;
    }

    // Part 8
    zmq::message_t messagePart8;
    socket->recv(&messagePart8);

    this->WriteMessageToFile(messagePart8, "start_7");

    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
    if (more == MORE_MESSAGES) {
      LOG4CXX_DEBUG(log, "Header has appendix");
      zmq::message_t messageAppendix;
      socket->recv(&messageAppendix);

      this->WriteMessageToFile(messageAppendix, "start_appendix");

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
      messageList.push_back(&newPart1message);
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
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleImageDataMessage(boost::shared_ptr<zmq::socket_t> socket, uint64_t frame_number) {
  LOG4CXX_DEBUG(log, "Handling Image Data Message");

  socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
  if (more != MORE_MESSAGES) {
    LOG4CXX_ERROR(log, "Image Data only contained 1 part");
    return;
  }

  // Add the current Acquisition ID to part 1 for easier downstream processing
  std::string part1WithAcquisitionID = AddAcquisitionIDToPart1();
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

  this->WriteMessageToFile(newPart1message, "image_" + PadInt(frame_number) + "_0");
  this->WriteMessageToFile(messagePart2, "image_" + PadInt(frame_number) + "_1");
  this->WriteMessageToFile(messagePart3, "image_" + PadInt(frame_number) + "_2");
  this->WriteMessageToFile(messagePart4, "image_" + PadInt(frame_number) + "_3");

  // Handle appendix
  socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
  if (more == MORE_MESSAGES) {
    LOG4CXX_DEBUG(log, "Image has appendix");
    zmq::message_t messageAppendix;
    socket->recv(&messageAppendix);

    this->WriteMessageToFile(messageAppendix, "image_" + PadInt(frame_number) + "_appendix");

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
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleEndOfSeriesMessage(boost::shared_ptr<zmq::socket_t> socket) {
  LOG4CXX_INFO(log, "Handling EndOfSeries Message");
  std::string part1WithAcquisitionID = AddAcquisitionIDToPart1();
  zmq::message_t newPart1message(part1WithAcquisitionID.size());
  memcpy (newPart1message.data (), part1WithAcquisitionID.c_str(), part1WithAcquisitionID.size());

  this->WriteMessageToFile(newPart1message, "end");

  SendMessageToAllConsumers(newPart1message);
  if (state != DSTR_IMAGE) {
    LOG4CXX_WARN(log, std::string("Received EndOfSeries message in unexpected state: ").append(GetStateString(state)));
  }
  state = DSTR_END;
  LOG4CXX_DEBUG(log, "Finished Handling EndOfSeries Message");
}

void EigerFan::WriteMessageToFile(zmq::message_t &message, std::string filename) {
  if (!this->devShmCache) {
    return;
  }

  boost::filesystem::path full_file_path = boost::filesystem::path(
    DEV_SHM_PATH + "/" + currentAcquisitionID + "/" + filename
  );
  boost::filesystem::path tmp_file_path = boost::filesystem::path(full_file_path.string() + ".tmp");

  boost::filesystem::create_directories(full_file_path.parent_path());

  int fd = open(tmp_file_path.c_str(), O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    LOG_WITH_ERRNO(log, "open failed");
  }

  if (write(fd, message.data(), message.size()) != message.size()) {
    LOG_WITH_ERRNO(log, "write failed");
  }

  close(fd);

  if (rename(tmp_file_path.c_str(), full_file_path.c_str())) {
    LOG_WITH_ERRNO(log, "rename failed");
  }
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

  // Get the second message part which contains the endpoint
  socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
  if (more != MORE_MESSAGES) {
    LOG4CXX_ERROR(log, "Monitor Message only contained 1 part");
  } else {
    zmq::message_t messagePart2;
    socket->recv(&messagePart2);
  }

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
    socket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
  }
  LOG4CXX_DEBUG(log, "Finished Handling Monitor Message");
}

/**
 * Handle messages from the monitoring of the forwarding zeromq connection
 *
 * Used to detect when forwarding consumer connects and disconnects from the EigerFan
 *
 * \param[in] message The zeromq message
 * \param[in] socket The socket that the message was received on
 */
void EigerFan::HandleForwardMonitorMessage(zmq::message_t &message, zmq::socket_t &socket) {
  LOG4CXX_DEBUG(log, "Handling Forward Monitor Message");

  // Get the event code from the message, which is a number contained in the first 16 bits
  uint16_t event = *(uint16_t *) (message.data());

  // Get the second message part which contains the endpoint
  socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
  if (more != MORE_MESSAGES) {
    LOG4CXX_ERROR(log, "Forward Monitor Message only contained 1 part");
  } else {
    zmq::message_t messagePart2;
    socket.recv(&messagePart2);
  }

  if (event == ZMQ_EVENT_ACCEPTED) {
    numConnectedForwardingSockets++;
    LOG4CXX_DEBUG(log, "Forwarding socket connected (" << numConnectedForwardingSockets << ")");

    if (WAITING_CONSUMERS != state && WAITING_STREAM != state) {
      LOG4CXX_ERROR(log, "Forwarding socket connected whilst in state: " << GetStateString(state));
    }

  } else if (event == ZMQ_EVENT_DISCONNECTED) {
    numConnectedForwardingSockets--;
    LOG4CXX_DEBUG(log, "Forwarding socket disconnected (" << numConnectedForwardingSockets << ")");
    if (WAITING_CONSUMERS != state && WAITING_STREAM != state) {
      LOG4CXX_ERROR(log, "Forwarding socket disconnected whilst in state: " << GetStateString(state));
    }
  }

  // Handle any message parts at the end
  socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
  while (more == MORE_MESSAGES) {
    zmq::message_t messagePartExtra;
    socket.recv(&messagePartExtra);
    LOG4CXX_ERROR(log, "Forward Monitor contained more parts than expected");
    socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
  }
  LOG4CXX_DEBUG(log, "Finished Handling Forward Monitor Message");
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

      // Add Number of messages received
      rapidjson::Value keyMessagesReceived("messages_received", document.GetAllocator());
      rapidjson::Value valueMessagesReceived(broker.messages_received());
      document.AddMember(keyMessagesReceived, valueMessagesReceived, document.GetAllocator());

      // Add Number of Frames sent
      rapidjson::Value keyFramesSent("frames_sent", document.GetAllocator());
      rapidjson::Value valueFramesSent(num_frames_sent);
      document.AddMember(keyFramesSent, valueFramesSent, document.GetAllocator());

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

    } else if (command.compare(CONTROL_REQ_CONFIG) == 0) {

      rapidjson::Document document;
      document.SetObject();

      // Add Number of threads
      rapidjson::Value keyNumThreads("num_threads", document.GetAllocator());
      rapidjson::Value valueNumThreads(config.num_threads);
      document.AddMember(keyNumThreads, valueNumThreads, document.GetAllocator());

      // Add Number of 0MQ context threads
      rapidjson::Value keyNumZMQContextThreads("num_zmq_context_threads", document.GetAllocator());
      rapidjson::Value valueNumZMQContextThreads(config.num_zmq_context_threads);
      document.AddMember(keyNumZMQContextThreads, valueNumZMQContextThreads, document.GetAllocator());

      // Add Number of 0MQ consumers
      rapidjson::Value keyNumConsumers("num_consumers", document.GetAllocator());
      rapidjson::Value valueNumConsumers(config.num_consumers);
      document.AddMember(keyNumConsumers, valueNumConsumers, document.GetAllocator());

      // Add control channel port
      rapidjson::Value keyCtrlPort("ctrl_channel_port", document.GetAllocator());
      rapidjson::Value valueCtrlPort(config.ctrl_channel_port, document.GetAllocator());
      document.AddMember(keyCtrlPort, valueCtrlPort, document.GetAllocator());

      // Add Eiger channel address
      rapidjson::Value keyEigerAddress("eiger_channel_address", document.GetAllocator());
      rapidjson::Value valueEigerAddress(config.eiger_channel_address, document.GetAllocator());
      document.AddMember(keyEigerAddress, valueEigerAddress, document.GetAllocator());

      // Add Eiger channel port
      rapidjson::Value keyEigerPort("eiger_channel_port", document.GetAllocator());
      rapidjson::Value valueEigerPort(config.eiger_channel_port, document.GetAllocator());
      document.AddMember(keyEigerPort, valueEigerPort, document.GetAllocator());

      // Add fan channel port start
      rapidjson::Value keyFanPort("fan_channel_port_start", document.GetAllocator());
      rapidjson::Value valueFanPort(config.fan_channel_port_start);
      document.AddMember(keyFanPort, valueFanPort, document.GetAllocator());

      // Add acquisition ID
      rapidjson::Value keyAcqID(CONTROL_ACQ_ID, document.GetAllocator());
      rapidjson::Value valueAcqID(configuredAcquisitionID, document.GetAllocator());
      document.AddMember(keyAcqID, valueAcqID, document.GetAllocator());

      // Add block size
      rapidjson::Value keyBlockSize(CONTROL_BLOCK_SIZE, document.GetAllocator());
      rapidjson::Value valueBlockSize(config.block_size);
      document.AddMember(keyBlockSize, valueBlockSize, document.GetAllocator());

      // Add configured offset value
      rapidjson::Value keyOffset(CONTROL_OFFSET, document.GetAllocator());
      rapidjson::Value valueOffset(configuredOffset);
      document.AddMember(keyOffset, valueOffset, document.GetAllocator());

      // Add forward stream value
      rapidjson::Value keyForward(CONTROL_FWD_STREAM, document.GetAllocator());
      rapidjson::Value valueForward;
      valueForward.SetBool(forwardStream);
      document.AddMember(keyForward, valueForward, document.GetAllocator());

      // Add /dev/shm cache state
      rapidjson::Value keyDevShmCache(CONTROL_DEV_SHM_CACHE, document.GetAllocator());
      rapidjson::Value valueDevShmCache;
      valueDevShmCache.SetBool(devShmCache);
      document.AddMember(keyDevShmCache, valueDevShmCache, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

      document.Accept(writer);

      std::ostringstream oss;
      oss << "{\"msg_type\":\"ack\",\"msg_val\":\"request_configuration\", \"params\": " << buffer.GetString() << "}";
      replyString.assign(oss.str());

    } else if (command.compare(CONTROL_REQ_VERSION) == 0) {

      rapidjson::Document document;
      document.SetObject();

      rapidjson::Document versionDocument;
      versionDocument.SetObject();

      rapidjson::Document subDocument;
      subDocument.SetObject();

      // Add Eiger channel address
      rapidjson::Value keyFull("full", subDocument.GetAllocator());
      rapidjson::Value valueFull(EIGER_DETECTOR_VERSION_STR, subDocument.GetAllocator());
      subDocument.AddMember(keyFull, valueFull, subDocument.GetAllocator());
      // Add Eiger channel address
      rapidjson::Value keyMajor("major", subDocument.GetAllocator());
      rapidjson::Value valueMajor(EIGER_DETECTOR_VERSION_MAJOR);
      subDocument.AddMember(keyMajor, valueMajor, subDocument.GetAllocator());
      // Add Eiger channel address
      rapidjson::Value keyMinor("minor", subDocument.GetAllocator());
      rapidjson::Value valueMinor(EIGER_DETECTOR_VERSION_MINOR);
      subDocument.AddMember(keyMinor, valueMinor, subDocument.GetAllocator());
      // Add Eiger channel address
      rapidjson::Value keyPatch("patch", subDocument.GetAllocator());
      rapidjson::Value valuePatch(EIGER_DETECTOR_VERSION_PATCH);
      subDocument.AddMember(keyPatch, valuePatch, subDocument.GetAllocator());
      // Add Eiger channel address
      rapidjson::Value keyShort("short", subDocument.GetAllocator());
      rapidjson::Value valueShort(EIGER_DETECTOR_VERSION_STR_SHORT, subDocument.GetAllocator());
      subDocument.AddMember(keyShort, valueShort, subDocument.GetAllocator());

      versionDocument.AddMember("eiger-detector", subDocument, versionDocument.GetAllocator());
      document.AddMember("version", versionDocument, document.GetAllocator());

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

      document.Accept(writer);

      std::ostringstream oss;
      oss << "{\"msg_type\":\"ack\",\"msg_val\":\"request_version\", \"params\": " << buffer.GetString() << "}";
      replyString.assign(oss.str());

    } else if (command.compare(CONTROL_CONFIGURE) == 0) {
      LOG4CXX_DEBUG(log, std::string("Handling Control Configure Message: ").append(jsonCommand));

      if (ctrlDocument.HasMember(CONTROL_PARAM_KEY.c_str())) {
        rapidjson::Value& paramsValue = ctrlDocument[CONTROL_PARAM_KEY.c_str()];
        replyString.assign(CONTROL_RESPONSE_NOCFGPARAM);
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
        }
        if (paramsValue.HasMember(CONTROL_OFFSET.c_str())) {
          // Change the consumer to send to based on an offset value
          configuredOffset = paramsValue[CONTROL_OFFSET.c_str()].GetInt();
          LOG4CXX_INFO(log, "Offset changed to " << configuredOffset);
          replyString.assign(CONTROL_RESPONSE_OK.c_str());
        }
        if (paramsValue.HasMember(CONTROL_ACQ_ID.c_str())) {
          // Change the acquisition ID
          configuredAcquisitionID = paramsValue[CONTROL_ACQ_ID.c_str()].GetString();
          LOG4CXX_INFO(log, "Acquisition ID changed to " << configuredAcquisitionID);
          replyString.assign(CONTROL_RESPONSE_OK.c_str());
        }
        if (paramsValue.HasMember(CONTROL_FWD_STREAM.c_str())) {
          // Change whether to forward the stream or not
          forwardStream = paramsValue[CONTROL_FWD_STREAM.c_str()].GetBool();
          LOG4CXX_INFO(log, "Forward stream changed to " << forwardStream);
          replyString.assign(CONTROL_RESPONSE_OK.c_str());
        }
        if (paramsValue.HasMember(CONTROL_DEV_SHM_CACHE.c_str())) {
          // Enable/disable /dev/shm cache
          devShmCache = paramsValue[CONTROL_DEV_SHM_CACHE.c_str()].GetBool();
          if (devShmCache) {
            LOG4CXX_INFO(log, "Enabling shared memory cache");
          } else {
            LOG4CXX_INFO(log, "Disabling shared memory cache");
            boost::filesystem::remove_all(DEV_SHM_PATH);
          }
          replyString.assign(CONTROL_RESPONSE_OK.c_str());
        }
        if (paramsValue.HasMember(CONTROL_BLOCK_SIZE.c_str())) {
          // Change the block size
          config.block_size = paramsValue[CONTROL_BLOCK_SIZE.c_str()].GetInt();
          LOG4CXX_INFO(log, "Block size changed to " << config.block_size);
          replyString.assign(CONTROL_RESPONSE_OK.c_str());
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
    controlSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
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

  //Send the message to the forwarding stream
  if (forwardStream && numConnectedForwardingSockets > 0)
  {
    zmq::message_t forwardingMessageCopy;
    forwardingMessageCopy.copy(&message);
    if (forwardSocket.send(forwardingMessageCopy, flags) == false) {
      LOG4CXX_ERROR(log, "Send socket returned false for forwarding socket");
    }
  }

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

  //Send the message to the forwarding stream
  if (forwardStream && numConnectedForwardingSockets > 0)
  {
    for (int messageCount = 0; messageCount < messageListSize; messageCount++)
    {
      zmq::message_t forwardingMessageCopy;
      forwardingMessageCopy.copy(messageList[messageCount]);
      if (messageCount != messageListSize - 1) {
        if (forwardSocket.send(forwardingMessageCopy, ZMQ_SNDMORE) == false) {
          LOG4CXX_ERROR(log, "Send socket returned false for forwarding socket");
        }
      } else {
        if (forwardSocket.send(forwardingMessageCopy) == false) {
          LOG4CXX_ERROR(log, "Send socket returned false for forwarding socket");
        }
      }
    }
  }

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
  LOG4CXX_DEBUG(log, "Sending message to single consumer at index:" << currentConsumerIndexToSendTo);

  //Send the message to the forwarding stream
  if (forwardStream && numConnectedForwardingSockets > 0)
  {
    zmq::message_t forwardingMessageCopy;
    forwardingMessageCopy.copy(&message);
    if (forwardSocket.send(forwardingMessageCopy, flags) == false) {
      LOG4CXX_ERROR(log, "Send socket returned false for forwarding socket");
    }
  }

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
 * Adds the current acquisition ID to the first message part.
 *
 * This is to enable easier downstream processing.
 * This relies on the class variable jsonDocument still containing the
 * first message part.
 *
 * \return The string of the first message part which now also contains the acquisition id
 */
std::string EigerFan::AddAcquisitionIDToPart1() {
  rapidjson::Value keyAcquisitionID(Eiger::ACQUISITION_ID_KEY.c_str(), jsonDocument.GetAllocator());
  rapidjson::Value valueAcquisitionID(currentAcquisitionID, jsonDocument.GetAllocator());
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
