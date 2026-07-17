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

#include <stdio.h>
#include <memory>

#ifdef SOMEIP_PLATFORM_STM32
#endif

#include "ThreadPool.hpp"
#include "ESomeIp.hpp"
#include "handler/Parser.hpp"
#include "config/config.hpp"
#include "handler/RxContext.hpp"
#include "handler/RxHandler.hpp"
#include "someIp/handler/Dispatcher.hpp"
#include "someIp/service_discovery/SdManager.hpp"
#include "someIp/communication/TransportRegistry.hpp"
#include "someIp/communication/PacketConv.hpp"
#include "config/logging_config.hpp"
#include "logging/BaseLogger.hpp"
#include "someIp/structs/PackageRxHandleResult.hpp"
#include "someIp/utils/ErrorCounters.hpp"

constexpr char THREAD_POOL_TAG[] = "THREAD_POOL";
using THREAD_POOL_LOGGER = BaseLogger<THREAD_POOL_TAG>;

namespace someIp {
  //tp::Reassembler tpReassembler; // one static instance
  ThreadPool::ThreadPool(TransportRegistry &ref_transports, RxContext *ptr_rxContext) : m_ref_transports(ref_transports), m_ptr_rxContext(ptr_rxContext){
    if(!m_incomingQueue.init()){
      THREAD_POOL_LOGGER::error("Failed to initialize incomingQueue");
      return;
    }
    if(!m_outgoingQueue.init()){
      THREAD_POOL_LOGGER::error("Failed to initialize outgoingQueue");
      return;
    }
    THREAD_POOL_LOGGER::log("ThreadPool created");
  }

  ThreadPool::~ThreadPool() {
    THREAD_POOL_LOGGER::log("ThreadPool destroying...");
    if(m_threadsRunning.get()){
      stop_threads();
    }
    else{
      THREAD_POOL_LOGGER::log("Threads already stopped.");
    }
    THREAD_POOL_LOGGER::log("ThreadPool destroyed");
  }

  bool ThreadPool::start_threads() {
    THREAD_POOL_LOGGER::log("Starting threads...");

    os::Guard lock(m_threadContolMutex);

    if(m_threadsRunning.get()){
      THREAD_POOL_LOGGER::log("Threads are already started");
      return true;
    }

    m_threadsRunning.set(true);
    int i = 0;

    for (auto &ref_r : m_receivers) {
      // count running before spawn returns to avoid a teardown race (report C-7)
      m_runningReceiverThreads.count_up();
      char name[16];
      std::snprintf(name, sizeof(name), "Receiver %d", i);
      ref_r = os::spawn(name, receiverThreadFunction, this, config::RECEIVER_THREAD_SIZE, config::RECEIVER_THREAD_PRIO);
      i++;
    }

    i = 0;

    for (auto &ref_s : m_senders) {
      m_runningSenderThreads.count_up();
      char name[16];
      std::snprintf(name, sizeof(name), "Sender %d", i);
      ref_s = os::spawn(name, senderThreadFunction, this, config::SENDER_THREAD_SIZE, config::SENDER_THREAD_PRIO);
      i++;
    }

    THREAD_POOL_LOGGER::log("Threads started");
    return true;
  }

  bool ThreadPool::stop_threads() {
    THREAD_POOL_LOGGER::log("Stopping threads...");
    os::Guard lock(m_threadContolMutex);

    if(!m_threadsRunning.get()){
      THREAD_POOL_LOGGER::log("Threads already stopped or not started yet");
      return true;
    }

    m_threadsRunning.set(false);

    // Unblock worker threads so they can exit their loops
    for (auto &ref_r : m_receivers){
      m_receiverNotificationSem.signal();
      os::sleep_ms(10);
    }

    for (auto &ref_s : m_senders){
      m_senderNotificationSem.signal();
      os::sleep_ms(10);
    }

    // lwIP threads are not joinable, so wait on the counters (report C-7)
    bool rx_done = m_runningReceiverThreads.wait_until_zero(500);
    bool tx_done = m_runningSenderThreads.wait_until_zero(500);

    if (!rx_done || !tx_done) {
      THREAD_POOL_LOGGER::error("stop_threads timed out waiting for workers (rx=%d tx=%d); tearing down anyway",
                                static_cast<int>(rx_done), static_cast<int>(tx_done));
      return false;
    }

    THREAD_POOL_LOGGER::log("Threads stopped");
    return true;
  }

  bool ThreadPool::add_buffer_to_queue(PackageRx &&ref_p){
    auto result = m_incomingQueue.moveElementIntoBuffer(std::move(ref_p));
    if(result){
      m_receiverNotificationSem.signal();
    } else {
      // RX queue full, count and log (report M-12)
      drop_counters().inc(DropCounter::rx_queue_full);
      THREAD_POOL_LOGGER::error("RX queue full; dropping received package");
    }

    return result;
  }

  bool ThreadPool::send_package(PackageTx &&ref_p){
    auto result = m_outgoingQueue.moveElementIntoBuffer(std::move(ref_p));
    if(result){
      m_senderNotificationSem.signal();
    } else {
      // TX queue full, count and log (report M-12)
      drop_counters().inc(DropCounter::tx_queue_full);
      THREAD_POOL_LOGGER::error("TX queue full; dropping outgoing package");
    }
    return result;
  }

  void ThreadPool::receiverThreadFunction(void *ptr_arg){
    auto ptr_pool = static_cast<ThreadPool *>(ptr_arg);
    if (ptr_pool == nullptr) {
      THREAD_POOL_LOGGER::error("nullptr passed to receiverThreadFunction\n");
      return;
    }

    ptr_pool->doReceiverWork();
  }

  void ThreadPool::senderThreadFunction(void *ptr_arg){
    auto ptr_pool = static_cast<ThreadPool *>(ptr_arg);
    if (ptr_pool == nullptr) {
      THREAD_POOL_LOGGER::error("nullptr passed to senderThreadFunction\n");
      return;
    }

    ptr_pool->doSenderWork();
  }

  void ThreadPool::doReceiverWork(){
    THREAD_POOL_LOGGER::log("Receiver thread started");

    handler::Dispatcher dispatcher(m_ptr_rxContext, &m_ptr_mng, m_ref_transports, m_ptr_ownerApi);

    while (m_threadsRunning.get())
    {
      bool isWorkToDo = false;
      PackageRx package = m_incomingQueue.moveFirstInto(&isWorkToDo);
      if(!isWorkToDo){
        m_receiverNotificationSem.wait();
        continue;
      }

      dispatcher.dispatch(std::move(package));
    }
    THREAD_POOL_LOGGER::log("Receiver thread terminated");
    m_runningReceiverThreads.count_down();
  }

  void ThreadPool::doSenderWork(){
    THREAD_POOL_LOGGER::log("Sender thread started");
    while (m_threadsRunning.get())
    {
      bool isWorkToDo = false;
      PackageTx package = m_outgoingQueue.moveFirstInto(&isWorkToDo);
      
      if(!isWorkToDo){
        m_senderNotificationSem.wait();
        continue;
      }

      IpAddr destIp     = *package.get_dest_ip();
      uint16_t port     = package.get_dest_port();
      IpEndpoint dest   = IpEndpoint(destIp.to_u32(), port);

      if(package.get_transport_mode() == TransportKind::UDP || package.get_transport_mode() == TransportKind::UDP_TP)
      {
        if(auto* ptr_udp = m_ref_transports.get(TransportKind::UDP)){
          ptr_udp->send_to(dest, packet_to_transport(std::move(package.payload)), package.get_src_port());
        }
      } else { // TCP
        auto* ptr_tcp = m_ref_transports.get(TransportKind::TCP);
        if(ptr_tcp){
          
          if(package.tcpConnection)
          {
            ptr_tcp->send_on(ConnectionId(TransportKind::TCP, package.tcpConnection), packet_to_transport(std::move(package.payload)));
          } else {
            // fallback to IP-based lookup when no connection handle
            ptr_tcp->send_to(dest, packet_to_transport(std::move(package.payload)));
          }
        }
        
      }
    }
    THREAD_POOL_LOGGER::log("Sender thread terminated");
    m_runningSenderThreads.count_down();
  }
};
