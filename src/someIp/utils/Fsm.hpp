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
#ifndef SOMEIP_FSM_HPP
#define SOMEIP_FSM_HPP

#include <cstddef>
#include "someIp/utils/PoolPtr.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

#include "someIp/os/Os.hpp"
#include "TimerScheduler.hpp"
#include "timer.hpp"
#include "someIp/utils/StaticQueue.hpp"
#include "someIp/utils/ErrorCounters.hpp"
#include "someIp/config/StackConfig.hpp"

namespace someIp {

struct FSMNoPayload {};

// no heap, returns buf so it can be used in init lists
inline const char *fsm_name(char *ptr_buf, std::size_t n, const char *ptr_prefix, unsigned id) {
  std::snprintf(ptr_buf, n, "%s%u", ptr_prefix, id);
  return ptr_buf;
}

// passive FSM, one thread drains the FIFO
// never post while holding a component mutex
template <class Ctx, class State, class Event, class Payload = FSMNoPayload>
class FSM {
public:
  using Action = void (Ctx::*)();
  using Observer = void (Ctx::*)(State from, Event event, State to);
  // called under the FSM mutex for DYNAMIC_TIMEOUT states
  using TimeoutProvider = long long (Ctx::*)(State entered);

  static constexpr long long DYNAMIC_TIMEOUT = -1;

  struct StateDef {
    long long timeout_ms;    // 0 disables timer, DYNAMIC_TIMEOUT asks the provider
    Event timeout_event;
  };

  struct Transition {
    State from;
    Event event;
    State to;
    Action action;           // nullptr = no-op
  };

  FSM(State initial, const StateDef *ptr_states, std::size_t numStates,
      const Transition *ptr_transitions, std::size_t numTransitions, Ctx *ptr_ctx,
      TimerScheduler &ref_timers, const char *ptr_name)
      : m_current(initial), m_ptr_states(ptr_states), m_numStates(numStates),
        m_ptr_transitions(ptr_transitions), m_numTransitions(numTransitions),
        m_ptr_ctx(ptr_ctx), m_ref_timers(ref_timers) {
    strncpy(m_name, ptr_name ? ptr_name : "", sizeof(m_name) - 1);
  }

  ~FSM() {
    stop();
  }

  FSM(const FSM &) = delete;
  FSM &operator=(const FSM &) = delete;
  FSM(FSM &&) = delete;
  FSM &operator=(FSM &&) = delete;

  State state() {
    os::Guard g(m_mutex);
    return m_current;
  }

  // enqueue and dispatch unless another thread is already dispatching
  void postEvent(Event e) { postEvent(e, Payload{}); }
  void postEvent(Event e, Payload p) {
    bool shouldDrain = false;
    {
      os::Guard g(m_mutex);
      if (m_stopped) {
        return;
      }
      if (!m_queue.push(QueuedEvent{e, std::move(p)})) {
        someIp::drop_counters().inc(someIp::DropCounter::fsm_queue_full); // report M-12
        return;
      }
      if (!m_dispatching) {
        m_dispatching = true;
        shouldDrain = true;
      }
    }
    if (shouldDrain) {
      drain();
    }
  }

  // backwards-compat alias
  void step(Event e) { postEvent(e); }
  void step(Event e, Payload p) { postEvent(e, std::move(p)); }

  // post only if the state matches, closes the state()/postEvent TOCTOU
  bool tryStep(State expected, Event e) { return tryStep(expected, e, Payload{}); }
  bool tryStep(State expected, Event e, Payload p) {
    bool shouldDrain = false;
    {
      os::Guard g(m_mutex);
      if (m_stopped || m_current != expected) {
        return false;
      }
      // report the drop instead of losing the event (M-12)
      if (!m_queue.push(QueuedEvent{e, std::move(p)})) {
        someIp::drop_counters().inc(someIp::DropCounter::fsm_queue_full);
        return false;
      }
      if (!m_dispatching) {
        m_dispatching = true;
        shouldDrain = true;
      }
    }
    if (shouldDrain) {
      drain();
    }
    return true;
  }

  // valid only inside the action or observer of that transition
  const Payload &currentPayload() const { return m_currentPayload; }

  // arm the initial state's timeout, no thread is launched
  void start() {
    os::Guard g(m_mutex);
    if (m_stopped) {
      return;
    }
    ++m_timerGen;
    armTimeoutLocked();
  }

  // makes the FSM inert, waits for in-flight timer callbacks (C-8)
  // never call from a TimerScheduler callback
  void stop() {
    someIp::PoolPtr<TimerEvent> sprt_timer;
    {
      os::Guard g(m_mutex);
      m_stopped = true;
      ++m_timerGen;
      sprt_timer = std::move(m_sprt_timer);
      m_sprt_timer = nullptr;
    }
    if (sprt_timer) {
      m_ref_timers.disable_timer(sprt_timer);
    }
  }

  void setObserver(Observer obs) {
    os::Guard g(m_mutex);
    m_observer = obs;
  }

  void setTimeoutProvider(TimeoutProvider provider) {
    os::Guard g(m_mutex);
    m_timeoutProvider = provider;
  }

private:
  void drain() {
    for (;;) {
      Action action = nullptr;
      Observer observer = nullptr;
      State from{};
      State to{};
      Event event{};
      bool haveTransition = false;
      someIp::PoolPtr<TimerEvent> sprt_oldTimer;
      {
        os::Guard g(m_mutex);
        if (m_stopped) {
          while (!m_queue.empty()) {
            m_queue.pop();
          }
          m_dispatching = false;
          return;
        }
        while (!m_queue.empty()) {
          QueuedEvent q = std::move(m_queue.front());
          m_queue.pop();
          const Transition *ptr_tr = findTransition(m_current, q.event);
          if (ptr_tr == nullptr) {
            continue;
          }
          from = m_current;
          to = ptr_tr->to;
          event = q.event;
          action = ptr_tr->action;
          m_current = to;
          m_currentPayload = std::move(q.payload);
          ++m_timerGen; // invalidate the old state's pending timeout
          sprt_oldTimer = std::move(m_sprt_timer);
          m_sprt_timer = nullptr;
          haveTransition = true;
          break;
        }
        if (!haveTransition) {
          m_dispatching = false;
          return;
        }
        observer = m_observer;
      }
      if (sprt_oldTimer) {
        sprt_oldTimer->disable();
      }
      // run these with the FSM mutex released
      if (action != nullptr) {
        (m_ptr_ctx->*action)();
      }
      if (observer != nullptr) {
        (m_ptr_ctx->*observer)(from, event, to);
      }
      // arm the new state's timeout only after the action ran
      {
        os::Guard g(m_mutex);
        if (!m_stopped) {
          armTimeoutLocked();
        }
      }
    }
  }

  // caller must hold m_mutex
  void armTimeoutLocked() {
    const StateDef &ref_sd = m_ptr_states[static_cast<std::size_t>(m_current)];
    long long ms = ref_sd.timeout_ms;
    if (ms == DYNAMIC_TIMEOUT && m_timeoutProvider != nullptr) {
      ms = (m_ptr_ctx->*m_timeoutProvider)(m_current);
    }
    if (m_sprt_timer) {
      // a disabled timer loses its callback, so arm a fresh one each time
      m_sprt_timer->disable();
      m_sprt_timer = nullptr;
    }
    if (ms <= 0) {
      return;
    }
    const uint64_t gen = m_timerGen;
    const Event timeoutEvent = ref_sd.timeout_event;
    // pooled timer node, no-heap callback
    m_sprt_timer = m_ref_timers.add_timer(
        m_name, static_cast<int>(ms),
        TimerCallback([this, gen, timeoutEvent]() { onTimerFire(gen, timeoutEvent); }),
        false);
  }

  // runs on the TimerScheduler worker thread
  void onTimerFire(uint64_t gen, Event timeoutEvent) {
    bool shouldDrain = false;
    {
      os::Guard g(m_mutex);
      if (m_stopped || gen != m_timerGen) {
        return; // stale timeout, the state moved on before expiry
      }
      if (!m_queue.push(QueuedEvent{timeoutEvent, Payload{}})) {
        someIp::drop_counters().inc(someIp::DropCounter::fsm_queue_full); // report M-12
        return;
      }
      if (!m_dispatching) {
        m_dispatching = true;
        shouldDrain = true;
      }
    }
    if (shouldDrain) {
      drain();
    }
  }

  const Transition *findTransition(State current, Event e) const {
    for (std::size_t i = 0; i < m_numTransitions; ++i) {
      const Transition &ref_tr = m_ptr_transitions[i];
      if (ref_tr.from != current || ref_tr.event != e) {
        continue;
      }
      return &ref_tr;
    }
    return nullptr;
  }

  struct QueuedEvent {
    Event event;
    Payload payload;
  };

  os::Mutex m_mutex;
  someIp::StaticQueue<QueuedEvent, someIp::config::MAX_FSM_EVENTS> m_queue;
  bool m_dispatching = false;
  bool m_stopped = false;
  uint64_t m_timerGen = 0;
  someIp::PoolPtr<TimerEvent> m_sprt_timer;

  State m_current;
  const StateDef *m_ptr_states;
  std::size_t m_numStates;
  const Transition *m_ptr_transitions;
  std::size_t m_numTransitions;
  Ctx *m_ptr_ctx;
  TimerScheduler &m_ref_timers;
  char m_name[config::FSM_NAME_SIZE] = {};
  Observer m_observer = nullptr;
  TimeoutProvider m_timeoutProvider = nullptr;
  Payload m_currentPayload{};
};

} // namespace someIp

#endif // SOMEIP_FSM_HPP
