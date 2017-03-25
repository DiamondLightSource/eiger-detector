/*!
 * eigerfan_main.cpp
 *
 */

#include <iostream>

#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
#include <log4cxx/xml/domconfigurator.h>

#include "EigerFan.h"

using namespace log4cxx;
using namespace log4cxx::helpers;


int main(int argc, char *argv[])
{
    //BasicConfigurator::configure();

    LoggerPtr log(Logger::getLogger("ED.APP"));;
    LOG4CXX_INFO(log, "Hello world! " << argv[0]);

    EigerFan eiger_fan;

    eiger_fan.run();

    return 0;
}

