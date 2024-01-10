#include <iostream>
#include <string>
#include <sstream>

#include <log4cxx/logger.h>  // getLogger

#include "MultiPullBroker.h"

MultiPullBroker::MultiPullBroker(
  std::string& sink_endpoint,
  int thread_count
) :
  sink_endpoint_(sink_endpoint),
  thread_count_(thread_count),
  messages_received_(0),
  shutdown_requested_(false)
{
  logger_ = log4cxx::Logger::getLogger("EigerFan.MultiPullBroker");
}

MultiPullBroker::~MultiPullBroker() {
  this->shutdown();
}

/**
 * Spawn worker threads to connect to endpoint
 *
 * \param[in] endpoint Endpoint of socket to pull data from
 * \param[in] inproc_context zmq context to create inproc socket in
 */
void MultiPullBroker::connect(std::string& endpoint, void* inproc_context) {
  // Store inproc context for workers to create sockets in
  this->inproc_context_ = static_cast<zmq::context_t*>(inproc_context);

  LOG4CXX_INFO(logger_, "Spawning " << this->thread_count_ << " worker threads");

  for (int _i = 0; _i < this->thread_count_; _i++) {
    worker_threads_.push_back(boost::shared_ptr<boost::thread>(
      new boost::thread(boost::bind(&MultiPullBroker::worker_loop, this, endpoint))
    ));
  }
}

/**
 * Entry point for worker threads
 *
 * \param[in] endpoint Endpoint of socket to pull data from
 */
void MultiPullBroker::worker_loop(std::string& endpoint) {
  int hwm = 10000;
  int linger = 100;

  // Create source in new isolated context
  // It is important to create a new context in each worker thread, as there are
  // throughput limitations to a context shared between threads. Increasing ZMQ IO
  // threads on the context is not sufficient.
  zmq::context_t source_context(1);
  zmq::socket_t source_socket(source_context, ZMQ_PULL);
  source_socket.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
  source_socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
  source_socket.connect(endpoint.c_str());

  // Create sink socket in context of main thread
  // The sink sockets must use the context from the main thread to use the inproc://
  // protocol. If it uses a different context the client will not see the messages.
  zmq::socket_t sink_socket(*this->inproc_context_, ZMQ_PUSH);
  sink_socket.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
  sink_socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
  sink_socket.connect(this->sink_endpoint_.c_str());

  // Initialise recv variables
  int more;
  size_t more_size = sizeof(more);

  // Run loop until asked to shutdown
  zmq::pollitem_t poll_items[] = {{source_socket, 0, ZMQ_POLLIN, 0}};
  while (!this->shutdown_requested_) {
    zmq::message_t message;

    // Receive multi-part messages from source and forward to sink
    more = 0;
    while (true) {
      zmq::poll(&poll_items[0], 1, 1000);
      if (!poll_items[0].revents & ZMQ_POLLIN) {
        if (more == 1) {
          LOG4CXX_WARN(this->logger_, "Timed out expecting more of multipart message");
        }
        break;
      }

      source_socket.recv(&message);

      source_socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
      if (more == 1) {
        sink_socket.send(message, ZMQ_SNDMORE);
      } else {
        sink_socket.send(message);
        ++this->messages_received_;
        break;
      }
    }
  }

  source_socket.close();
  sink_socket.close();
}

/** Start the message counter, having received the first message
 *
 */
void MultiPullBroker::start_message_counter() {
  this->messages_received_ = 1;
}

/** Return number of messages received
 *
 */
uint64_t MultiPullBroker::messages_received()
{
  return this->messages_received_;
}

/**
 * Request to stop worker threads and shutdown
 */
void MultiPullBroker::shutdown() {
  if (this->shutdown_requested_) {
    return;
  }

  this->shutdown_requested_ = true;
  for (int i = 0; i < this->worker_threads_.size(); ++i) {
    this->worker_threads_[i]->join();
  }
}
