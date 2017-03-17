/*
 * eigerfan_unittest.cpp
 *
 */
#define BOOST_TEST_MODULE "EigerFanUnitTests"
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <log4cxx/logger.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/simplelayout.h>


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

BOOST_AUTO_TEST_CASE( EigerFanTest )
{
    BOOST_TEST_MESSAGE("Hello unittest world");


    BOOST_CHECK_EQUAL(1, 1);
    BOOST_CHECK_EQUAL(1, 2);
}

BOOST_AUTO_TEST_SUITE_END();

