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

#ifndef SOMEIP_TIMERSCHEDULER_HPP_
#define SOMEIP_TIMERSCHEDULER_HPP_

#include <cstdio>
#include <functional>
#include <vector>
#include <memory>
#include <algorithm>

#include "someIp/os/Os.hpp"
#include "AtomicBool.hpp"
#include "someIp/config/config.hpp"
#include "timer.hpp"
#include "someIp/utils/PoolAllocator.hpp"
#include "someIp/utils/PoolPtr.hpp"
#include "someIp/utils/PoolTrace.hpp"
#include "someIp/utils/StaticVector.hpp"
#include "someIp/config/StackConfig.hpp"

class TimerScheduler
{
public:
    TimerScheduler()
    {
    }

    ~TimerScheduler()
    {
        stop();
    }

    TimerScheduler(const TimerScheduler &) = delete;
    TimerScheduler &operator=(const TimerScheduler &) = delete;

    void start()
    {
        if (m_started.get())
        {
            return;
        }
        m_started.set(true);
        someIp::os::spawn("timer_sched",
                          worker_thread_fn,
                          this,
                          someIp::config::EVENT_MANAGER_THREAD_SIZE,
                          someIp::config::EVENT_MANAGER_THREAD_PRIO);
    }

    void stop()
    {
        if (!m_started.get())
        {
            return;
        }
        m_shutdown.set(true);
        // lwIP threads cannot be joined, worker signals this semaphore before exiting
        m_stopped_sem.wait();
        m_started.set(false);
        m_shutdown.set(false);
    }

    void add_timer(const someIp::PoolPtr<TimerEvent> &ref_timer)
    {
        someIp::os::Guard lock(m_mtx);
        m_sprt_timers.push_back(ref_timer);
        // std::cout << "[EVENT_MANAGER]: add timer with duration: " << timer->get_duration() << std::endl;
    }

    someIp::PoolPtr<TimerEvent> add_timer(const char *ptr_name, int milliseconds, TimerCallback cb, bool periodic = false)
    {
        TimerType type = periodic ? TimerType::Periodic : TimerType::single_shot;
        // pooled so scheduling does not heap-allocate
        static someIp::FixedBlockPool<someIp::config::TIMER_POOL_BLOCK_SIZE, someIp::config::MAX_TIMERS> pool;
        auto sprt_timer = someIp::pool_ptr_make<TimerEvent>(pool, someIp::pool_id::TIMER, "TimerScheduler",
                                                        ptr_name, Duration(milliseconds), type, std::move(cb));
        {
            someIp::os::Guard lock(m_mtx);
            m_sprt_timers.push_back(sprt_timer);
        }
        return sprt_timer;
    }

    // blocks until any in-flight callback finishes, not callable from the worker
    void disable_timer(someIp::PoolPtr<TimerEvent> sprt_timer)
    {
        if (!sprt_timer) return;
        sprt_timer->disable();

        {
            someIp::os::Guard lock(m_mtx);
            ++m_cb_waiters;
        }
        while (sprt_timer->is_callback_running())
        {
            m_cb_done_sem.wait();
        }
        {
            someIp::os::Guard lock(m_mtx);
            --m_cb_waiters;
        }

        remove_timer_locked(sprt_timer);
    }

    // disable and unlist without waiting for an in-flight callback (report M-16)
    void cancel_timer_nowait(someIp::PoolPtr<TimerEvent> sprt_timer)
    {
        if (!sprt_timer) return;
        sprt_timer->disable();
        remove_timer_locked(sprt_timer);
    }

    void run_loop()
    {
        while (!m_shutdown.get())
        {
            // avoid busy waiting
            someIp::os::sleep_ms(10);

            // collect expiring timers, no heap and bounded per tick
            someIp::StaticVector<std::pair<someIp::PoolPtr<TimerEvent>, TimerCallback>, someIp::config::MAX_TIMERS_PER_TICK> sprt_timers_to_fire;

            {
                someIp::os::Guard lock(m_mtx);

                for (auto &ref_timer : m_sprt_timers)
                {
                    auto [should_fire, callback] = ref_timer->check_expiry();
                    if (should_fire && callback)
                    {
                        if (!sprt_timers_to_fire.push_back({ref_timer, std::move(callback)})) break; // tick full, rest fire next tick
                    }
                }
            }

            // run callbacks without holding the mutex
            for (auto &[timer, callback] : sprt_timers_to_fire)
            {
                if (callback)
                {
                    callback();
                    // std::cout << "Timer " << timer->get_name() << " finished successfully" << std::endl;
                }
                else
                {
                    printf("Timer %s has invalid callback\n", timer->get_name());
                }
                // mark complete then wake disable_timer waiters (report C-8)
                timer->callback_completed();
                {
                    someIp::os::Guard lock(m_mtx);
                    for (int i = 0; i < m_cb_waiters; ++i) m_cb_done_sem.signal();
                }
            }

            // drop inactive timers
            {
                someIp::os::Guard lock(m_mtx);

                m_sprt_timers.erase(
                    std::remove_if(m_sprt_timers.begin(), m_sprt_timers.end(),
                                   [](const someIp::PoolPtr<TimerEvent> &ref_timer)
                                   {
                                       return !ref_timer->get_state() && !ref_timer->is_callback_running();
                                   }),
                    m_sprt_timers.end());
            }
        }

        cleanup_on_shutdown();
    }

private:
    static void worker_thread_fn(void *ptr_arg)
    {
        auto *ptr_self = static_cast<TimerScheduler *>(ptr_arg);
        ptr_self->run_loop();
        ptr_self->m_stopped_sem.signal();
    }

    void remove_timer_locked(const someIp::PoolPtr<TimerEvent> &ref_timer)
    {
        someIp::os::Guard lock(m_mtx);
        m_sprt_timers.erase(
            std::remove_if(m_sprt_timers.begin(), m_sprt_timers.end(),
                           [&ref_timer](const someIp::PoolPtr<TimerEvent> &ref_t)
                           {
                               return ref_t == ref_timer;
                           }),
            m_sprt_timers.end());
    }

    void cleanup_on_shutdown()
    {
        // std::cout << "[EVENT_MANAGER]: Starting shutdown cleanup..." << std::endl;

        {
            someIp::os::Guard lock(m_mtx);
            for (auto &ref_timer : m_sprt_timers)
            {
                ref_timer->disable();
            }
        }

        bool callbacks_running = true;
        while (callbacks_running)
        {
            someIp::os::sleep_ms(10);
            callbacks_running = false;

            someIp::os::Guard lock(m_mtx);
            for (auto &ref_timer : m_sprt_timers)
            {
                if (ref_timer->is_callback_running())
                {
                    callbacks_running = true;
                    break;
                }
            }
        }

        {
            someIp::os::Guard lock(m_mtx);
            m_sprt_timers.clear();
        }

        // std::cout << "[EVENT_MANAGER]: Shutdown cleanup completed" << std::endl;
    }

    someIp::StaticVector<someIp::PoolPtr<TimerEvent>, someIp::config::MAX_TIMERS> m_sprt_timers;
    someIp::os::Mutex m_mtx;
    someIp::os::Semaphore m_stopped_sem;
    someIp::os::Semaphore m_cb_done_sem;   // signalled after a callback completes
    int m_cb_waiters{0};                   // guarded by m_mtx
    AtomicBool m_started{false};
    AtomicBool m_shutdown{false};
};

#endif // SOMEIP_TIMERSCHEDULER_HPP_
