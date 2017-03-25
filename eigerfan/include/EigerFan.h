//
// Created by up45 on 23/03/17.
//

#ifndef EIGERDAQ_EIGERFAN_H
#define EIGERDAQ_EIGERFAN_H

#include <log4cxx/logger.h>


class EigerFan {
public:
    virtual ~EigerFan();

public:
    EigerFan();

    void run();
private:
    log4cxx::LoggerPtr log;
};


#endif //EIGERDAQ_EIGERFAN_H
