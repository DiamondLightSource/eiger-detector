#include <zmq/zmq.hpp>
#include <stdlib.h>
#include <iostream>
#include <sstream>

/**
 * Listens to a ZMQ stream on port 9999 at the address specified
 * and writes out the data received on the stream to separate files
 *
 * The first (and only) argument to the program is an optional address,
 * with 'localhost' being the default if no address specified.
 *
 * e.g. ./streamWriter 192.168.0.1
 *
 */
int main (int argc, char *argv[]) {

  std::string address("localhost");

  if (argc == 2) {
    address = argv[1];
  }

  std::string connectAddress("tcp://" + address + ":9999");

  std::cout << "Connecting to " << connectAddress << "..." << std::endl;

  zmq::context_t context(1);

  //  Socket to receive messages on
  zmq::socket_t receiver(context, ZMQ_PULL);
  receiver.connect(connectAddress.c_str());

  bool firstMsg = true;

  long messageReceived = 1;

  std::cout << "Waiting for messages..." << std::endl;

  //  Listen forever
  while (1) {

    zmq::message_t message;

    receiver.recv(&message);

    std::ostringstream oss;
    oss << "streamfile_" << messageReceived;

    std::cout << "Message received: Number = [" << messageReceived << "] Size = [" << message.size() << "] File = [" << oss.str() << "]" <<  std::endl;

    FILE * pFile;
    pFile = fopen (oss.str().c_str(), "wb");
    fwrite (message.data() , sizeof(char), message.size(), pFile);
    fclose (pFile);
    messageReceived++;

  }
  return 0;
}
