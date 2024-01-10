/*
 *  Created on: 18/08/2023
 *      Author: Gary Yendell
 */

#ifndef MULTIPULLBROKER_H
#define MULTIPULLBROKER_H


#include <atomic>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <log4cxx/logger.h>
#include "zmq/zmq.hpp"

class MultiPullBroker {

public:
  MultiPullBroker(
    std::string& sink_endpoint,
    int thread_count
  );
  ~MultiPullBroker();

  void connect(std::string& endpoint, void* inproc_context);
  void start_message_counter();
  uint64_t messages_received();
  void shutdown();

protected:

private:
  log4cxx::LoggerPtr logger_;

  std::vector<boost::shared_ptr<boost::thread> > worker_threads_;
  std::string source_endpoint_;
  std::string sink_endpoint_;
  zmq::context_t* inproc_context_;
  int thread_count_;
  std::atomic<std::uint64_t> messages_received_;
  bool shutdown_requested_;

  void worker_loop(std::string& endpoint);

};

#endif // MULTIPULLBROKER_H
