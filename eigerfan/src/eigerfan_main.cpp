/*!
 * eigerfan_main.cpp
 *
 */

#include <iostream>
#include "EigerFan.h"

int main(int argc, char *argv[]) {

    EigerFan eigerFan;

    // TODO configure with arguments

    eigerFan.SetNumberOfConsumers(3);

    eigerFan.Start();

    return 0;
}

