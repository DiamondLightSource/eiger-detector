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
	std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
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
	std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
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
	found = replyMessage2.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":1,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
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
	std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
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
	found = replyMessage2.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":1,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
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
	found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":2,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
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
	zmq::socket_t  eigerStream(context, ZMQ_PUSH);
	eigerStream.bind("tcp://*:9999");

	std::string globalHeader("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}");

	zmq::message_t streamMessage(globalHeader.size());
	memcpy (streamMessage.data (), globalHeader.c_str(), globalHeader.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received the header message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage3(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_HEADER and 1 consumer
	std::size_t found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_HEADER\",\"num_conn\":1,\"series\":1,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage3);

	// Check the consumer has been sent the header
	zmq::message_t consumerMessage;
	receiver.recv (&consumerMessage);
	std::string consumerMessageValue(static_cast<char*>(consumerMessage.data()), consumerMessage.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}", consumerMessageValue);

	// Check there aren't more messages that have been sent to the consumer
	int more;
	size_t more_size = sizeof (more);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Send image data
	std::string imgData1("{\"htype\":\"dimage-1.0\", \"series\": 1, \"frame\": 324, \"hash\": \"fc67f000d08fe6b380ea9434b8362d22\"}");
	std::string imgData2("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}");
	std::string imgData3("IMGDATA");
	std::string imgData4("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}");
	streamMessage.rebuild(imgData1.size());
	memcpy (streamMessage.data (), imgData1.c_str(), imgData1.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData2.size());
	memcpy (streamMessage.data (), imgData2.c_str(), imgData2.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData3.size());
	memcpy (streamMessage.data (), imgData3.c_str(), imgData3.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData4.size());
	memcpy (streamMessage.data (), imgData4.c_str(), imgData4.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received an image data message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage4(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_IMAGE and 1 consumer
	found = replyMessage4.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_IMAGE\",\"num_conn\":1,\"series\":1,\"frame\":324,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage4);

	// Check the consumer has been sent the image data
	zmq::message_t consumerMessageI1;
	receiver.recv (&consumerMessageI1);
	std::string consumerMessageI1Value(static_cast<char*>(consumerMessageI1.data()), consumerMessageI1.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage-1.0\",\"series\":1,\"frame\":324,\"hash\":\"fc67f000d08fe6b380ea9434b8362d22\",\"acqID\":\"\"}", consumerMessageI1Value);
	zmq::message_t consumerMessageI2;
	receiver.recv (&consumerMessageI2);
	std::string consumerMessageI2Value(static_cast<char*>(consumerMessageI2.data()), consumerMessageI2.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}", consumerMessageI2Value);
	zmq::message_t consumerMessageI3;
	receiver.recv (&consumerMessageI3);
	std::string consumerMessageI3Value(static_cast<char*>(consumerMessageI3.data()), consumerMessageI3.size());
	BOOST_CHECK_EQUAL("IMGDATA", consumerMessageI3Value);
	zmq::message_t consumerMessageI4;
	receiver.recv (&consumerMessageI4);
	std::string consumerMessageI4Value(static_cast<char*>(consumerMessageI4.data()), consumerMessageI4.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}", consumerMessageI4Value);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Send end of series message
	std::string endOfStream("{\"htype\": \"dseries_end-1.0\", \"series\": 1}");
	streamMessage.rebuild(endOfStream.size());
	memcpy (streamMessage.data (), endOfStream.c_str(), endOfStream.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received an image data message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage5(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_END or WAITING_STREAM and 1 consumer
	found = replyMessage5.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_END\",\"num_conn\":1,\"series\":1,\"frame\":324,\"fan_offset\":0},\"timestamp\":");
	std::size_t found2 = replyMessage5.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":1,\"series\":1,\"frame\":324,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos || found2!=std::string::npos, "Not in correct state after EndOfSeries message: " << replyMessage5);

	// Check the consumer has been sent the end of series message
	zmq::message_t consumerMessageEnd;
	receiver.recv (&consumerMessageEnd);
	std::string consumerMessageEndValue(static_cast<char*>(consumerMessageEnd.data()), consumerMessageEnd.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dseries_end-1.0\",\"series\":1,\"acqID\":\"\"}", consumerMessageEndValue);

	shutdownEigerFan();
    eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckFanSendsMultiImages )
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
	zmq::socket_t  eigerStream(context, ZMQ_PUSH);
	eigerStream.bind("tcp://*:9999");

	std::string globalHeader("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}");

	zmq::message_t streamMessage(globalHeader.size());
	memcpy (streamMessage.data (), globalHeader.c_str(), globalHeader.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received the header message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage3(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_HEADER and 1 consumer
	std::size_t found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_HEADER\",\"num_conn\":1,\"series\":1,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage3);

	// Check the consumer has been sent the header
	zmq::message_t consumerMessage;
	receiver.recv (&consumerMessage);
	std::string consumerMessageValue(static_cast<char*>(consumerMessage.data()), consumerMessage.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}", consumerMessageValue);

	// Check there aren't more messages that have been sent to the consumer
	int more;
	size_t more_size = sizeof (more);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Send image data
	std::string imgData1("{\"htype\":\"dimage-1.0\", \"series\": 1, \"frame\": 324, \"hash\": \"fc67f000d08fe6b380ea9434b8362d22\"}");
	std::string imgData2("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}");
	std::string imgData3("IMGDATA");
	std::string imgData4("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}");
	streamMessage.rebuild(imgData1.size());
	memcpy (streamMessage.data (), imgData1.c_str(), imgData1.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData2.size());
	memcpy (streamMessage.data (), imgData2.c_str(), imgData2.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData3.size());
	memcpy (streamMessage.data (), imgData3.c_str(), imgData3.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData4.size());
	memcpy (streamMessage.data (), imgData4.c_str(), imgData4.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received an image data message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage4(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_IMAGE and 1 consumer
	found = replyMessage4.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_IMAGE\",\"num_conn\":1,\"series\":1,\"frame\":324,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage4);

	// Check the consumer has been sent the image data
	zmq::message_t consumerMessageI1;
	receiver.recv (&consumerMessageI1);
	std::string consumerMessageI1Value(static_cast<char*>(consumerMessageI1.data()), consumerMessageI1.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage-1.0\",\"series\":1,\"frame\":324,\"hash\":\"fc67f000d08fe6b380ea9434b8362d22\",\"acqID\":\"\"}", consumerMessageI1Value);
	zmq::message_t consumerMessageI2;
	receiver.recv (&consumerMessageI2);
	std::string consumerMessageI2Value(static_cast<char*>(consumerMessageI2.data()), consumerMessageI2.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}", consumerMessageI2Value);
	zmq::message_t consumerMessageI3;
	receiver.recv (&consumerMessageI3);
	std::string consumerMessageI3Value(static_cast<char*>(consumerMessageI3.data()), consumerMessageI3.size());
	BOOST_CHECK_EQUAL("IMGDATA", consumerMessageI3Value);
	zmq::message_t consumerMessageI4;
	receiver.recv (&consumerMessageI4);
	std::string consumerMessageI4Value(static_cast<char*>(consumerMessageI4.data()), consumerMessageI4.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}", consumerMessageI4Value);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Send second lot of image data
	std::string imgData21("{\"htype\":\"dimage-1.0\", \"series\": 1, \"frame\": 325, \"hash\": \"fc67f000d08fe6b380ea9434b8362d22\"}");
	std::string imgData22("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398248}");
	std::string imgData23("IMGDATA2");
	std::string imgData24("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834261, \"stop_time\": 834760834282, \"real_time\": 1000000}");
	streamMessage.rebuild(imgData21.size());
	memcpy (streamMessage.data (), imgData21.c_str(), imgData21.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData22.size());
	memcpy (streamMessage.data (), imgData22.c_str(), imgData22.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData23.size());
	memcpy (streamMessage.data (), imgData23.c_str(), imgData23.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData24.size());
	memcpy (streamMessage.data (), imgData24.c_str(), imgData24.size());
	eigerStream.send(streamMessage);
	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan is still in same state
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage5(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_IMAGE and 1 consumer
	found = replyMessage5.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_IMAGE\",\"num_conn\":1,\"series\":1,\"frame\":325,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage5);

	// Check the consumer has been sent the image data
	zmq::message_t consumerMessageI21;
	receiver.recv (&consumerMessageI21);
	std::string consumerMessageI21Value(static_cast<char*>(consumerMessageI21.data()), consumerMessageI21.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage-1.0\",\"series\":1,\"frame\":325,\"hash\":\"fc67f000d08fe6b380ea9434b8362d22\",\"acqID\":\"\"}", consumerMessageI21Value);
	zmq::message_t consumerMessageI22;
	receiver.recv (&consumerMessageI22);
	std::string consumerMessageI22Value(static_cast<char*>(consumerMessageI22.data()), consumerMessageI22.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398248}", consumerMessageI22Value);
	zmq::message_t consumerMessageI23;
	receiver.recv (&consumerMessageI23);
	std::string consumerMessageI23Value(static_cast<char*>(consumerMessageI23.data()), consumerMessageI23.size());
	BOOST_CHECK_EQUAL("IMGDATA2", consumerMessageI23Value);
	zmq::message_t consumerMessageI24;
	receiver.recv (&consumerMessageI24);
	std::string consumerMessageI24Value(static_cast<char*>(consumerMessageI24.data()), consumerMessageI24.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834261, \"stop_time\": 834760834282, \"real_time\": 1000000}", consumerMessageI24Value);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Send end of series message
	std::string endOfStream("{\"htype\": \"dseries_end-1.0\", \"series\": 1}");
	streamMessage.rebuild(endOfStream.size());
	memcpy (streamMessage.data (), endOfStream.c_str(), endOfStream.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received an image data message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage6(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_END or WAITING_STREAM and 1 consumer
	found = replyMessage6.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_END\",\"num_conn\":1,\"series\":1,\"frame\":325,\"fan_offset\":0},\"timestamp\":");
	std::size_t found2 = replyMessage6.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":1,\"series\":1,\"frame\":325,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos || found2!=std::string::npos, "Not in correct state after EndOfSeries message: " << replyMessage6);

	// Check the consumer has been sent the end of series message
	zmq::message_t consumerMessageEnd;
	receiver.recv (&consumerMessageEnd);
	std::string consumerMessageEndValue(static_cast<char*>(consumerMessageEnd.data()), consumerMessageEnd.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dseries_end-1.0\",\"series\":1,\"acqID\":\"\"}", consumerMessageEndValue);

	shutdownEigerFan();
    eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckFanSendsMultiImagesMultiConsumers )
{
	EigerFan eigerFan;
	eigerFan.SetNumberOfConsumers(2);
    boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

    zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_DEALER);
	socket.connect ("tcp://localhost:5559");

	// Build the command to get the status
	std::string command("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"status\", \"params\": {}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
	zmq::message_t request (command.size());
	zmq::message_t reply;

	// Now connect the consumers
	zmq::socket_t receiver1(context, ZMQ_PULL);
	receiver1.connect("tcp://localhost:31600");
	zmq::socket_t receiver2(context, ZMQ_PULL);
	receiver2.connect("tcp://localhost:31601");

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Start up an Emulated Eiger stream and send the Global Header Message
	zmq::socket_t  eigerStream(context, ZMQ_PUSH);
	eigerStream.bind("tcp://*:9999");

	std::string globalHeader("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}");

	zmq::message_t streamMessage(globalHeader.size());
	memcpy (streamMessage.data (), globalHeader.c_str(), globalHeader.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received the header message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage3(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_HEADER and 2 consumers
	std::size_t found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_HEADER\",\"num_conn\":2,\"series\":1,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage3);

	// Check that both consumers have been sent the header
	zmq::message_t consumerMessage11;
	receiver1.recv (&consumerMessage11);
	std::string consumerMessageValue11(static_cast<char*>(consumerMessage11.data()), consumerMessage11.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}", consumerMessageValue11);

	// Check there aren't more messages that have been sent to the consumer
	int more;
	size_t more_size = sizeof (more);
	receiver1.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	zmq::message_t consumerMessage21;
	receiver2.recv (&consumerMessage21);
	std::string consumerMessageValue21(static_cast<char*>(consumerMessage21.data()), consumerMessage21.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}", consumerMessageValue21);

	// Check there aren't more messages that have been sent to the consumer
	receiver2.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Send image data 1
	std::string imgData1("{\"htype\":\"dimage-1.0\", \"series\": 1, \"frame\": 324, \"hash\": \"fc67f000d08fe6b380ea9434b8362d22\"}");
	std::string imgData2("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}");
	std::string imgData3("IMGDATA");
	std::string imgData4("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}");
	streamMessage.rebuild(imgData1.size());
	memcpy (streamMessage.data (), imgData1.c_str(), imgData1.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData2.size());
	memcpy (streamMessage.data (), imgData2.c_str(), imgData2.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData3.size());
	memcpy (streamMessage.data (), imgData3.c_str(), imgData3.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData4.size());
	memcpy (streamMessage.data (), imgData4.c_str(), imgData4.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check the first consumer has been sent the image data
	zmq::message_t consumerMessageI11;
	receiver1.recv (&consumerMessageI11);
	std::string consumerMessageI11Value(static_cast<char*>(consumerMessageI11.data()), consumerMessageI11.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage-1.0\",\"series\":1,\"frame\":324,\"hash\":\"fc67f000d08fe6b380ea9434b8362d22\",\"acqID\":\"\"}", consumerMessageI11Value);
	zmq::message_t consumerMessageI12;
	receiver1.recv (&consumerMessageI12);
	std::string consumerMessageI12Value(static_cast<char*>(consumerMessageI12.data()), consumerMessageI12.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}", consumerMessageI12Value);
	zmq::message_t consumerMessageI13;
	receiver1.recv (&consumerMessageI13);
	std::string consumerMessageI13Value(static_cast<char*>(consumerMessageI13.data()), consumerMessageI13.size());
	BOOST_CHECK_EQUAL("IMGDATA", consumerMessageI13Value);
	zmq::message_t consumerMessageI14;
	receiver1.recv (&consumerMessageI14);
	std::string consumerMessageI14Value(static_cast<char*>(consumerMessageI14.data()), consumerMessageI14.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}", consumerMessageI14Value);
	receiver1.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Check the second consumer has not been sent any image data
	zmq::message_t consumerMessageI221;
	bool success = receiver2.recv (&consumerMessageI221, ZMQ_NOBLOCK);
	BOOST_CHECK_EQUAL(false, success);

	// Send second lot of image data
	std::string imgData21("{\"htype\":\"dimage-1.0\", \"series\": 1, \"frame\": 325, \"hash\": \"fc67f000d08fe6b380ea9434b8362d22\"}");
	std::string imgData22("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398248}");
	std::string imgData23("IMGDATA2");
	std::string imgData24("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834261, \"stop_time\": 834760834282, \"real_time\": 1000000}");
	streamMessage.rebuild(imgData21.size());
	memcpy (streamMessage.data (), imgData21.c_str(), imgData21.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData22.size());
	memcpy (streamMessage.data (), imgData22.c_str(), imgData22.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData23.size());
	memcpy (streamMessage.data (), imgData23.c_str(), imgData23.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData24.size());
	memcpy (streamMessage.data (), imgData24.c_str(), imgData24.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check the second consumer has been sent the image data
	zmq::message_t consumerMessageI21;
	receiver2.recv (&consumerMessageI21);
	std::string consumerMessageI21Value(static_cast<char*>(consumerMessageI21.data()), consumerMessageI21.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage-1.0\",\"series\":1,\"frame\":325,\"hash\":\"fc67f000d08fe6b380ea9434b8362d22\",\"acqID\":\"\"}", consumerMessageI21Value);
	zmq::message_t consumerMessageI22;
	receiver2.recv (&consumerMessageI22);
	std::string consumerMessageI22Value(static_cast<char*>(consumerMessageI22.data()), consumerMessageI22.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398248}", consumerMessageI22Value);
	zmq::message_t consumerMessageI23;
	receiver2.recv (&consumerMessageI23);
	std::string consumerMessageI23Value(static_cast<char*>(consumerMessageI23.data()), consumerMessageI23.size());
	BOOST_CHECK_EQUAL("IMGDATA2", consumerMessageI23Value);
	zmq::message_t consumerMessageI24;
	receiver2.recv (&consumerMessageI24);
	std::string consumerMessageI24Value(static_cast<char*>(consumerMessageI24.data()), consumerMessageI24.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834261, \"stop_time\": 834760834282, \"real_time\": 1000000}", consumerMessageI24Value);
	receiver2.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check the first consumer has not been sent any image data
	zmq::message_t consumerMessageI121;
	success = receiver1.recv (&consumerMessageI121, ZMQ_NOBLOCK);
	BOOST_CHECK_EQUAL(false, success);

	// Send end of series message
	std::string endOfStream("{\"htype\": \"dseries_end-1.0\", \"series\": 1}");
	streamMessage.rebuild(endOfStream.size());
	memcpy (streamMessage.data (), endOfStream.c_str(), endOfStream.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check that both consumers have been sent the end of series message
	zmq::message_t consumerMessageEnd1;
	receiver1.recv (&consumerMessageEnd1);
	std::string consumerMessageEnd1Value(static_cast<char*>(consumerMessageEnd1.data()), consumerMessageEnd1.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dseries_end-1.0\",\"series\":1,\"acqID\":\"\"}", consumerMessageEnd1Value);

	zmq::message_t consumerMessageEnd2;
	receiver2.recv (&consumerMessageEnd2);
	std::string consumerMessageEnd2Value(static_cast<char*>(consumerMessageEnd2.data()), consumerMessageEnd2.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dseries_end-1.0\",\"series\":1,\"acqID\":\"\"}", consumerMessageEnd2Value);

	shutdownEigerFan();
    eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckClose )
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
	std::size_t found = replyMessage.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
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
	found = replyMessage2.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":1,\"series\":0,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage2);

	// Connect an eiger and send the header
	zmq::socket_t  eigerStream(context, ZMQ_PUSH);
	eigerStream.bind("tcp://*:9999");

	std::string globalHeader("{\"htype\":\"dheader-1.0\", \"series\": 27, \"header_detail\": \"none\"}");

	zmq::message_t streamMessage(globalHeader.size());
	memcpy (streamMessage.data (), globalHeader.c_str(), globalHeader.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check the consumer has been sent the header
	zmq::message_t consumerMessage;
	receiver.recv (&consumerMessage);
	std::string consumerMessageValue(static_cast<char*>(consumerMessage.data()), consumerMessage.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dheader-1.0\", \"series\": 27, \"header_detail\": \"none\"}", consumerMessageValue);

	// Check there aren't more messages that have been sent to the consumer
	int more;
	size_t more_size = sizeof (more);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	std::string command2("{\"msg_type\": \"cmd\", \"id\": 1, \"msg_val\": \"configure\", \"params\": {\"close\":true}, \"timestamp\": \"2017-07-03T14:17:58.440432\"}");
	zmq::message_t request2 (command2.size());
	memcpy (request2.data (), command2.c_str(), command2.size());
	socket.send (request2);

	//  Get the reply.
	zmq::message_t reply3;
	socket.recv (&reply3);
	std::string replyMessage3(static_cast<char*>(reply3.data()), reply3.size());

	found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"configure\",\"params\":{},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage3);

	// Check the EndOfSeries message is sent
	zmq::message_t consumerMessageEnd;
	receiver.recv (&consumerMessageEnd);
	std::string consumerMessageEndValue(static_cast<char*>(consumerMessageEnd.data()), consumerMessageEnd.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dseries_end-1.0\",\"series\":27,\"acqID\":\"\"}", consumerMessageEndValue);

	eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckFanSendsAcquisitionID )
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
	zmq::socket_t  eigerStream(context, ZMQ_PUSH);
	eigerStream.bind("tcp://*:9999");

	std::string globalHeader("{\"htype\":\"dheader-1.0\", \"series\": 1, \"header_detail\": \"none\"}");

	zmq::message_t streamMessage(globalHeader.size());
	memcpy (streamMessage.data (), globalHeader.c_str(), globalHeader.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);

	// Send the acquisition ID throught the global header appendix
	std::string globalHeaderAppendix("{\"acqID\":\"test_acq_id\"}");
	streamMessage.rebuild(globalHeaderAppendix.size());
	memcpy (streamMessage.data (), globalHeaderAppendix.c_str(), globalHeaderAppendix.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received the header message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage3(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_HEADER and 1 consumer
	std::size_t found = replyMessage3.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_HEADER\",\"num_conn\":1,\"series\":1,\"frame\":0,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage3);

	// Check the consumer has been sent the header WITH THE ACQUISITION ID APPENDED
	zmq::message_t consumerMessage;
	receiver.recv (&consumerMessage);
	std::string consumerMessageValue(static_cast<char*>(consumerMessage.data()), consumerMessage.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dheader-1.0\",\"series\":1,\"header_detail\":\"none\",\"acqID\":\"test_acq_id\"}", consumerMessageValue);

	// Check there is another message that has been sent to the consumer
	int more;
	size_t more_size = sizeof (more);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);

	BOOST_CHECK_EQUAL(1, more);
	zmq::message_t consumerMessageGHA;
	receiver.recv (&consumerMessageGHA);
	std::string consumerMessageGHAValue(static_cast<char*>(consumerMessageGHA.data()), consumerMessageGHA.size());
	BOOST_CHECK_EQUAL("{\"acqID\":\"test_acq_id\"}", consumerMessageGHAValue);

	// Check there aren't more messages that have been sent to the consumer
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Send image data
	std::string imgData1("{\"htype\":\"dimage-1.0\", \"series\": 1, \"frame\": 324, \"hash\": \"fc67f000d08fe6b380ea9434b8362d22\"}");
	std::string imgData2("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}");
	std::string imgData3("IMGDATA");
	std::string imgData4("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}");
	streamMessage.rebuild(imgData1.size());
	memcpy (streamMessage.data (), imgData1.c_str(), imgData1.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData2.size());
	memcpy (streamMessage.data (), imgData2.c_str(), imgData2.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData3.size());
	memcpy (streamMessage.data (), imgData3.c_str(), imgData3.size());
	eigerStream.send(streamMessage, ZMQ_SNDMORE);
	streamMessage.rebuild(imgData4.size());
	memcpy (streamMessage.data (), imgData4.c_str(), imgData4.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received an image data message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage4(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_IMAGE and 1 consumer
	found = replyMessage4.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_IMAGE\",\"num_conn\":1,\"series\":1,\"frame\":324,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos, replyMessage4);

	// Check the consumer has been sent the image data WITH THE ACQUISITION ID APPENDED
	zmq::message_t consumerMessageI1;
	receiver.recv (&consumerMessageI1);
	std::string consumerMessageI1Value(static_cast<char*>(consumerMessageI1.data()), consumerMessageI1.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage-1.0\",\"series\":1,\"frame\":324,\"hash\":\"fc67f000d08fe6b380ea9434b8362d22\",\"acqID\":\"test_acq_id\"}", consumerMessageI1Value);
	zmq::message_t consumerMessageI2;
	receiver.recv (&consumerMessageI2);
	std::string consumerMessageI2Value(static_cast<char*>(consumerMessageI2.data()), consumerMessageI2.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dimage_d-1.0\", \"shape\":[1030,1065], \"type\": \"uint32\", \"encoding\": \"lz4<\", \"size\": 47398247}", consumerMessageI2Value);
	zmq::message_t consumerMessageI3;
	receiver.recv (&consumerMessageI3);
	std::string consumerMessageI3Value(static_cast<char*>(consumerMessageI3.data()), consumerMessageI3.size());
	BOOST_CHECK_EQUAL("IMGDATA", consumerMessageI3Value);
	zmq::message_t consumerMessageI4;
	receiver.recv (&consumerMessageI4);
	std::string consumerMessageI4Value(static_cast<char*>(consumerMessageI4.data()), consumerMessageI4.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dconfig-1.0\", \"start_time\": 834759834260, \"stop_time\": 834760834280, \"real_time\": 1000000}", consumerMessageI4Value);
	receiver.getsockopt(ZMQ_RCVMORE, &more, &more_size);
	BOOST_CHECK_EQUAL(0, more);

	// Send end of series message
	std::string endOfStream("{\"htype\": \"dseries_end-1.0\", \"series\": 1}");
	streamMessage.rebuild(endOfStream.size());
	memcpy (streamMessage.data (), endOfStream.c_str(), endOfStream.size());
	eigerStream.send(streamMessage);

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, having received an image data message and sent out to consumer
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	reply.rebuild();
	socket.recv (&reply);
	std::string replyMessage5(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to DSTR_END or WAITING_STREAM and 1 consumer
	found = replyMessage5.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"DSTR_END\",\"num_conn\":1,\"series\":1,\"frame\":324,\"fan_offset\":0},\"timestamp\":");
	std::size_t found2 = replyMessage5.find("{\"msg_type\":\"ack\",\"msg_val\":\"status\",\"params\":{\"state\":\"WAITING_STREAM\",\"num_conn\":1,\"series\":1,\"frame\":324,\"fan_offset\":0},\"timestamp\":");
	BOOST_CHECK_MESSAGE(found!=std::string::npos || found2!=std::string::npos, "Not in correct state after EndOfSeries message: " << replyMessage5);

	// Check the consumer has been sent the end of series message WITH THE ACQUISITION ID APPENDED
	zmq::message_t consumerMessageEnd;
	receiver.recv (&consumerMessageEnd);
	std::string consumerMessageEndValue(static_cast<char*>(consumerMessageEnd.data()), consumerMessageEnd.size());
	BOOST_CHECK_EQUAL("{\"htype\":\"dseries_end-1.0\",\"series\":1,\"acqID\":\"test_acq_id\"}", consumerMessageEndValue);

	shutdownEigerFan();
    eigerfanThread.join();
}

BOOST_AUTO_TEST_SUITE_END();

