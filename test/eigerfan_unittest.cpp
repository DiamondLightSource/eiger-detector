/*
 * eigerfan_unittest.cpp
 *
 */
#define BOOST_TEST_MODULE "EigerFanUnitTests"
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


class EigerFanTestFixture
{
public:
    EigerFanTestFixture() :
        logger(log4cxx::Logger::getLogger("EigerFanUnitTest"))
    {

    }
    log4cxx::LoggerPtr logger;


};
BOOST_FIXTURE_TEST_SUITE(EigerFanUnitTest, EigerFanTestFixture);

void startEigerFan(EigerFan &eigerFan) {
	eigerFan.Start();
}

void shutdownEigerFan() {
	zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REQ);

	socket.connect ("tcp://localhost:5559");

	std::string command("KILL");
	zmq::message_t request (command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	//  Get the reply.
	zmq::message_t reply;
	socket.recv (&reply);
	std::string replyMessage(static_cast<char*>(reply.data()), reply.size());
	std::cout << "rece " << replyMessage;
}

BOOST_AUTO_TEST_CASE( EigerFanTest )
{
    BOOST_TEST_MESSAGE("Hello unittest world");

    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckKill )
{
	EigerFan eigerFan;
    boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

    zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REQ);

	socket.connect ("tcp://localhost:5559");

	// Send the kill command
	std::string command("KILL");
	zmq::message_t request (command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	//  Get the reply.
	zmq::message_t reply;
	socket.recv (&reply);
	std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

	BOOST_CHECK_EQUAL("OK",replyMessage);

    eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckGetStatus )
{
	EigerFan eigerFan;
    boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

    zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REQ);
	socket.connect ("tcp://localhost:5559");

	// Send the command to get the status
	std::string command("STATUS");
	zmq::message_t request (command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	// Get the reply
	zmq::message_t reply;
	socket.recv (&reply);
	std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

	// Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and no consumers
	BOOST_CHECK_EQUAL("{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0}", replyMessage);

	shutdownEigerFan();
    eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheckConsumerConnect )
{
	EigerFan eigerFan;
	eigerFan.SetNumberOfConsumers(1);
    boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

    zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REQ);
	socket.connect ("tcp://localhost:5559");

	// Send the command to get the status
	std::string command("STATUS");
	zmq::message_t request (command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	// Get the reply
	zmq::message_t reply;
	socket.recv (&reply);
	std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

	// Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and no consumers
	BOOST_CHECK_EQUAL("{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0}", replyMessage);

	// Now connect a consumer
	zmq::socket_t receiver(context, ZMQ_PULL);
	receiver.connect("tcp://localhost:5557");

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, waiting for stream
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	socket.recv (&reply);
	std::string replyMessage2(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to WAITING_STREAM and 1 consumer
	BOOST_CHECK_EQUAL("{\"state\":\"WAITING_STREAM\",\"num_conn\":1}", replyMessage2);

	shutdownEigerFan();
    eigerfanThread.join();
}

BOOST_AUTO_TEST_CASE( EigerFanTestCheck2ConsumersConnect )
{
	EigerFan eigerFan;
	eigerFan.SetNumberOfConsumers(2);
    boost::thread eigerfanThread(startEigerFan, boost::ref(eigerFan));

    zmq::context_t context (1);
	zmq::socket_t socket (context, ZMQ_REQ);
	socket.connect ("tcp://localhost:5559");

	// Send the command to get the status
	std::string command("STATUS");
	zmq::message_t request (command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	// Get the reply
	zmq::message_t reply;
	socket.recv (&reply);
	std::string replyMessage(static_cast<char*>(reply.data()), reply.size());

	// Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and no consumers
	BOOST_CHECK_EQUAL("{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":0}", replyMessage);

	// Now connect a consumer
	zmq::socket_t receiver(context, ZMQ_PULL);
	receiver.connect("tcp://localhost:5557");

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has NOT gone into different state, and is still waiting for consumers
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	socket.recv (&reply);
	std::string replyMessage2(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to WAITING_CONSUMERS and 1 consumers
	BOOST_CHECK_EQUAL("{\"state\":\"WAITING_CONSUMERS\",\"num_conn\":1}", replyMessage2);

	// Now connect a second consumer
	zmq::socket_t receiver2(context, ZMQ_PULL);
	receiver2.connect("tcp://localhost:5557");

	// Sleep to give time for 0MQ message to get through and be processed
	sleep(1);

	// Check EigerFan has gone into different state, waiting for stream
	request.rebuild(command.size());
	memcpy (request.data (), command.c_str(), command.size());
	socket.send (request);

	socket.recv (&reply);
	std::string replyMessage3(static_cast<char*>(reply.data()), reply.size());
	// Expect the status returned to be a json string, with the state set to WAITING_STREAM and 2 consumers
	BOOST_CHECK_EQUAL("{\"state\":\"WAITING_STREAM\",\"num_conn\":2}", replyMessage3);

	shutdownEigerFan();
    eigerfanThread.join();
}

BOOST_AUTO_TEST_SUITE_END();

