//
// Created by up45 on 23/03/17.
//
#include "EigerFan.h"

EigerFan::EigerFan() {
    this->log = log4cxx::Logger::getLogger("ED.EigerFan");
    LOG4CXX_INFO(log, "Creating EigerFan object");
}

EigerFan::~EigerFan() {

}

void EigerFan::run() {
    LOG4CXX_INFO(log, "EigerFan::run()");
}
