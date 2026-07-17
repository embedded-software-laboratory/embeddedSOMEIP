/*
The MIT License
Copyright (c) 2026 Lehrstuhl Informatik 11 - RWTH Aachen University
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE

This file is part of embeddedSOMEIP.

Author: i11 - Embedded Software, RWTH Aachen University
*/

#ifndef SOMEIP_THREADPOOL_HPP
#define SOMEIP_THREADPOOL_HPP

#include "someIp/os/Os.hpp"
#include <memory>

#include "config/config.hpp"
#include "someIp/Types.hpp"
#include "someIp/structs/Package.hpp"
#include "someIp/structs/PackageRx.hpp"
#include "someIp/structs/PackageTx.hpp"
#include "someIp/storages/ThreadSafeCircularBuffer.hpp"
#include "someIp/communication/TransportRegistry.hpp"
#include "someIp/handler/IServiceHandler.hpp"
#include "someIp/handler/RxContext.hpp"
#include "someIp/utils/AtomicBool.hpp"
#include "someIp/utils/CounterThreadSafe.hpp"

#include "someIp/transport-protocol/Reassembler.hpp"

namespace someIp {
  namespace sd {
    class SdManager;
  }
  class ESomeIp;

// worker threads for asynchronous SOME/IP message processing
class ThreadPool {
  public:
  ThreadPool(TransportRegistry &ref_transports, RxContext *ptr_rxContext);

  ~ThreadPool();

  // spawn the receiver and sender threads
  bool start_threads();

  // gracefully stop all worker threads
  bool stop_threads();

  // enqueue a received package for processing
  bool add_buffer_to_queue(PackageRx &&ref_p);

  // enqueue a package for transmission
  bool send_package(PackageTx &&ref_p);

  void set_sd_manager(sd::SdManager *ptr_manager){ m_ptr_mng = ptr_manager; }

  // the owning eSomeIP, used by the receive Dispatcher
  void set_owner_api(ESomeIp *ptr_api){ m_ptr_ownerApi = ptr_api; }

  static const size_t MAX_TASKS = 100;

  private:
  TransportRegistry &m_ref_transports;
  const RxContext *m_ptr_rxContext;

  os::Mutex m_threadContolMutex;
  AtomicBool m_threadsRunning{false};
  CounterThreadSafe m_runningReceiverThreads{};
  CounterThreadSafe m_runningSenderThreads{};
  std::array<os::ThreadHandle, config::NUMBER_OF_RECEIVER_THREADS> m_receivers;
  std::array<os::ThreadHandle, config::NUMBER_OF_SENDER_THREADS> m_senders;

  os::Semaphore m_receiverNotificationSem;
  os::Semaphore m_senderNotificationSem;

  ThreadSafeCircularBuffer<PackageTx, config::SENDER_QUEUE_LENGTH> m_outgoingQueue;
  ThreadSafeCircularBuffer<PackageRx, config::RECEIVER_QUEUE_LENGTH> m_incomingQueue;

  sd::SdManager *m_ptr_mng = nullptr; // initialized via setter
  ESomeIp *m_ptr_ownerApi = nullptr;       // initialized via setter

  // static entry point for receiver threads
  static void receiverThreadFunction(void *ptr_arg);

  // static entry point for sender threads
  static void senderThreadFunction(void *ptr_arg);

  // main loop for processing incoming messages
  void doReceiverWork();

  // main loop for transmitting outgoing messages
  void doSenderWork();
};

}
#endif // SOMEIP_THREADPOOL_HPP
