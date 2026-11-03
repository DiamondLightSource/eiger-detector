/*
 * eigerfan_unittest.cpp
 *
 */
#define BOOST_TEST_MODULE "EigerFanUnitTest"
#define BOOST_TEST_MAIN

#include <iostream>
#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <log4cxx/logger.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/simplelayout.h>
#include "zmq/zmq.hpp"
#include "EigerFan.h"

#include <EigerFan.h>

#include "stream2.h"
#include "cbor.h"

struct GlobalConfig	{
  GlobalConfig() {
    log4cxx::BasicConfigurator::configure();
    BOOST_TEST_MESSAGE("Global config");
  }
  ~GlobalConfig(){}
};
BOOST_GLOBAL_FIXTURE(GlobalConfig);

class EigerFanTestFixture
{
public:
  EigerFanTestFixture() :
    logger(log4cxx::Logger::getLogger("ED.UnitTest"))
  {
    EigerFan eiger_fan;
  }
  log4cxx::LoggerPtr logger;


};
BOOST_FIXTURE_TEST_SUITE(EigerFanUnitTest, EigerFanTestFixture);

void startEigerFan(EigerFan &eigerFan) {
  eigerFan.run();
}

void shutdownEigerFan() {
  zmq::context_t context (1);
  zmq::socket_t socket (context, ZMQ_DEALER);

  socket.connect ("tcp://localhost:5559");

  std::string command("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"configure\", \"params\": {\"kill\":true}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
  zmq::message_t request (command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  //  Get the reply.
  zmq::message_t reply;
  socket.recv (&reply);
  std::string replyMessage(static_cast<char*>(reply.data()), reply.size());
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckKill )
{
  EigerFan eigerFan;
  boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

  zmq::context_t context (1);
  zmq::socket_t socket (context, ZMQ_DEALER);

  socket.connect ("tcp://localhost:5559");

  // Send the kill command
  std::string command("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"configure\", \"params\": {\"kill\":true}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
  zmq::message_t request (command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  //  Get the reply.
  zmq::message_t reply;
  socket.recv (&reply);
  std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

  std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"configure\",\"params\":{},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage);

  eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckGetStatus )
{
  EigerFan eigerFan;
  boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

  zmq::context_t context (1);
  zmq::socket_t socket (context, ZMQ_DEALER);
  socket.connect ("tcp://localhost:5559");

  // Send the command to get the status
  std::string command("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"status\", \"params\": {}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
  zmq::message_t request (command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  // Get the reply
  zmq::message_t reply;
  socket.recv (&reply);
  std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

  // Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and no consumers
  std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0,\"series\":0,\"frame\":0,\"frames_sent\":0,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage);

  shutdownEigerFan();
  eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckConsumerConnect )
{
  EigerFan eigerFan;
  eigerFan.SetNumberOfConsumers(1);
  boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

  zmq::context_t context (1);
  zmq::socket_t socket (context, ZMQ_DEALER);
  socket.connect ("tcp://localhost:5559");

  // Send the command to get the status
  std::string command("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"status\", \"params\": {}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
  zmq::message_t request (command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  // Get the reply
  zmq::message_t reply;
  socket.recv (&reply);
  std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

  // Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and no consumers
  std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0,\"series\":0,\"frame\":0,\"frames_sent\":0,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage);

  // Now connect a consumer
  zmq::socket_t receiver(context, ZMQ_PULL);
  receiver.connect("tcp://localhost:31600");

  // Sleep to give time for 0MQ message to get through and be processed
  sleep(1);

  // Check EigerFan has gone into different state, waiting for stream
  request.rebuild(command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  socket.recv (&reply);
  std::string replyMessage2(static_cast<char*>(reply.data()), reply.size());
  // Expect the status returned to be a json string, with the state set to WAITING_STREAM and 1 consumer
  found = replyMessage2.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":1,\"series\":0,\"frame\":0,\"frames_sent\":0,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage2);

  shutdownEigerFan();
  eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheck2ConsumersConnect )
{
  EigerFan eigerFan;
  eigerFan.SetNumberOfConsumers(2);
  boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

  zmq::context_t context (1);
  zmq::socket_t socket (context, ZMQ_DEALER);
  socket.connect ("tcp://localhost:5559");

  // Send the command to get the status
  std::string command("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"status\", \"params\": {}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
  zmq::message_t request (command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  // Get the reply
  zmq::message_t reply;
  socket.recv (&reply);
  std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

  // Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and no consumers
  std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0,\"series\":0,\"frame\":0,\"frames_sent\":0,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage);

  // Now connect a consumer
  zmq::socket_t receiver(context, ZMQ_PULL);
  receiver.connect("tcp://localhost:31600");

  // Sleep to give time for 0MQ message to get through and be processed
  sleep(1);

  // Check EigerFan has NOT gone into different state, and is still waiting for consumers
  request.rebuild(command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  socket.recv (&reply);
  std::string replyMessage2(static_cast<char*>(reply.data()), reply.size());
  // Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and 1 consumers
  found = replyMessage2.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":1,\"series\":0,\"frame\":0,\"frames_sent\":0,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage2);

  // Now connect a second consumer
  zmq::socket_t receiver2(context, ZMQ_PULL);
  receiver2.connect("tcp://localhost:31601");

  // Sleep to give time for 0MQ message to get through and be processed
  sleep(1);

  // Check EigerFan has gone into different state, waiting for stream
  request.rebuild(command.size());
  memcpy (request.data (), command.c_str(), command.size());
  socket.send (request);

  socket.recv (&reply);
  std::string replyMessage3(static_cast<char*>(reply.data()), reply.size());
  // Expect the status returned to be a json string, with the state set to WAITING_STREAM and 2 consumers
  found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":2,\"series\":0,\"frame\":0,\"frames_sent\":0,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage3);

  shutdownEigerFan();
  eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckFanSends )
{
  EigerFan eigerFan;
  eigerFan.SetNumberOfConsumers(1);
  boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

  zmq::context_t context (1);
  zmq::socket_t socket (context, ZMQ_DEALER);
  socket.connect ("tcp://localhost:5559");

  // Build the command to get the status
  std::string command("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"status\", \"params\": {}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
  zmq::message_t request (command.size());
  zmq::message_t reply;

  // Now connect a consumer
  zmq::socket_t receiver(context, ZMQ_PULL);
  receiver.connect("tcp://localhost:31600");

  // Sleep to give time for 0MQ message to get through and be processed
  sleep(1);

  // Start up an Emulated Eiger stream and send the Global Header Message
  zmq::socket_t eigerStream(context, ZMQ_PUSH);
  eigerStream.bind("tcp://*:31001");

  // Send the start message
  uint8_t cbor_start_buffer[45];
  cbor_start_buffer[0] = 0xd9;
  cbor_start_buffer[1] = 0xd9;
  cbor_start_buffer[2] = 0xf7;
  CborEncoder startEncoder, startMapEncoder;
  cbor_encoder_init(&startEncoder, cbor_start_buffer + 3, sizeof(cbor_start_buffer) - 3, 0);
  cbor_encoder_create_map(&startEncoder, &startMapEncoder, 3);
  cbor_encode_text_stringz(&startMapEncoder, "type");
  cbor_encode_text_stringz(&startMapEncoder, "start");
  cbor_encode_text_stringz(&startMapEncoder, "series_id");
  cbor_encode_int(&startMapEncoder, 1);
  cbor_encode_text_stringz(&startMapEncoder, "series_unique_id");
  cbor_encode_text_stringz(&startMapEncoder, "1");
  cbor_encoder_close_container(&startEncoder, &startMapEncoder);

  zmq::message_t startMessage(sizeof(cbor_start_buffer));
  memcpy(startMessage.data(), (void*) cbor_start_buffer, sizeof(cbor_start_buffer));
  std::string startMessageValue(static_cast<char*>(startMessage.data()), startMessage.size());
  eigerStream.send(startMessage);

  // Sleep to give time for 0MQ message to get through and be processed
  sleep(1);

  // Check EigerFan has gone into different state, having received the header message and sent out to consumer
  request.rebuild(command.size());
  memcpy(request.data (), command.c_str(), command.size());
  socket.send (request);

  reply.rebuild();
  socket.recv(&reply);
  std::string replyMessage1(static_cast<char*>(reply.data()), reply.size());
  // Expect the status returned to be a json string, with the state set to DSTR_HEADER and 1 consumer
  std::size_t found = replyMessage1.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_HEADER\",\"num_conn\":1,\"series\":1,\"frame\":0,\"frames_sent\":0,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found != std::string::npos, replyMessage1);

  // Check the consumer has been forwarded the start message
  zmq::message_t consumerStartMessage;
  receiver.recv(&consumerStartMessage);
  std::string consumerStartMessageValue(static_cast<char*>(consumerStartMessage.data()), consumerStartMessage.size());
  BOOST_CHECK_EQUAL(startMessageValue, consumerStartMessageValue);

  // Send image message
  uint8_t cbor_image_buffer[57];
  cbor_image_buffer[0] = 0xd9;
  cbor_image_buffer[1] = 0xd9;
  cbor_image_buffer[2] = 0xf7;
  CborEncoder imageEncoder, imageMapEncoder;
  cbor_encoder_init(&imageEncoder, cbor_image_buffer + 3, sizeof(cbor_image_buffer) - 3, 0);
  cbor_encoder_create_map(&imageEncoder, &imageMapEncoder, 3);
  cbor_encode_text_stringz(&imageMapEncoder, "type");
  cbor_encode_text_stringz(&imageMapEncoder, "image");
  cbor_encode_text_stringz(&imageMapEncoder, "image_id");
  cbor_encode_int(&imageMapEncoder, 324);
  cbor_encode_text_stringz(&imageMapEncoder, "series_id");
  cbor_encode_int(&imageMapEncoder, 1);
  cbor_encode_text_stringz(&imageMapEncoder, "series_unique_id");
  cbor_encode_text_stringz(&imageMapEncoder, "1");
  cbor_encoder_close_container(&imageEncoder, &imageMapEncoder);

  zmq::message_t imageMessage(sizeof(cbor_image_buffer));
  memcpy(imageMessage.data(), (void*) cbor_image_buffer, sizeof(cbor_image_buffer));
  std::string imageMessageValue(static_cast<char*>(imageMessage.data()), imageMessage.size());
  eigerStream.send(imageMessage);

  // Sleep to give time for 0MQ message to get through and be processed
  sleep(1);

  // Check EigerFan has gone into different state, having received an image message and sent out to consumer
  request.rebuild(command.size());
  memcpy(request.data(), command.c_str(), command.size());
  socket.send (request);

  reply.rebuild();
  socket.recv(&reply);
  std::string replyMessage2(static_cast<char*>(reply.data()), reply.size());
  // Expect the status returned to be a json string, with the state set to DSTR_IMAGE and 1 consumer
  found = replyMessage2.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_IMAGE\",\"num_conn\":1,\"series\":1,\"frame\":324,\"frames_sent\":1,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found != std::string::npos, replyMessage2);

  // Check the consumer has been sent the image message
  zmq::message_t consumerImageMessage;
  receiver.recv(&consumerImageMessage);
  std::string consumerImageMessageValue(static_cast<char*>(consumerImageMessage.data()), consumerImageMessage.size());
  BOOST_CHECK_EQUAL(imageMessageValue, consumerImageMessageValue);

  // Send end message
  uint8_t cbor_end_buffer[45];
  cbor_end_buffer[0] = 0xd9;
  cbor_end_buffer[1] = 0xd9;
  cbor_end_buffer[2] = 0xf7;
  CborEncoder endEncoder, endMapEncoder;
  cbor_encoder_init(&endEncoder, cbor_end_buffer + 3, sizeof(cbor_end_buffer) - 3, 0);
  cbor_encoder_create_map(&endEncoder, &endMapEncoder, 3);
  cbor_encode_text_stringz(&endMapEncoder, "type");
  cbor_encode_text_stringz(&endMapEncoder, "end");
  cbor_encode_text_stringz(&endMapEncoder, "series_id");
  cbor_encode_int(&endMapEncoder, 1);
  cbor_encode_text_stringz(&endMapEncoder, "series_unique_id");
  cbor_encode_text_stringz(&endMapEncoder, "1");
  cbor_encoder_close_container(&endEncoder, &endMapEncoder);

  zmq::message_t endMessage(sizeof(cbor_end_buffer));
  memcpy(endMessage.data(), (void*) cbor_end_buffer, sizeof(cbor_end_buffer));
  std::string endMessageValue(static_cast<char*>(endMessage.data()), endMessage.size());
  eigerStream.send(endMessage);

  // Sleep to give time for 0MQ message to get through and be processed
  sleep(1);

  // Check EigerFan has gone into different state, having received an image data message and sent out to consumer
  request.rebuild(command.size());
  memcpy(request.data(), command.c_str(), command.size());
  socket.send(request);

  reply.rebuild();
  socket.recv(&reply);
  std::string replyMessage3(static_cast<char*>(reply.data()), reply.size());
  // Expect the status returned to be a json string, with the state set to DSTR_END or WAITING_STREAM and 1 consumer
  found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_END\",\"num_conn\":1,\"series\":1,\"frame\":324,\"frames_sent\":1,\"fan_offset\":0},\"timestamp\":");
  std::size_t found2 = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":1,\"series\":1,\"frame\":324,\"frames_sent\":1,\"fan_offset\":0},\"timestamp\":");
  BOOST_CHECK_MESSAGE(found != std::string::npos || found2 != std::string::npos, "Not in correct state after EndOfSeries message: " << replyMessage2);

  // Check the consumer has been sent the end message
  zmq::message_t consumerEndMessage;
  receiver.recv(&consumerEndMessage);
  std::string consumerEndMessageValue(static_cast<char*>(consumerEndMessage.data()), consumerEndMessage.size());
  BOOST_CHECK_EQUAL(endMessageValue, consumerEndMessageValue);

  shutdownEigerFan();
  eigerfanThread.join();
}

BOOST_AUTO_TEST_SUITE_END();

