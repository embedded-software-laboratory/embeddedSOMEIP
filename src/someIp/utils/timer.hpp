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

#ifndef SOMEIP_TIMER_HPP_
#define SOMEIP_TIMER_HPP_

#include <chrono>
#include <cstring>
#include <cstdint>
#include <utility>

#include "someIp/os/Os.hpp"
#include "someIp/utils/InplaceFunction.hpp"
#include "someIp/config/StackConfig.hpp"

// millisecond duration alias
using Duration = std::chrono::milliseconds;

// no-heap timer callback
using TimerCallback = someIp::InplaceFunction<void(), someIp::config::TIMER_CB_SIZE>;

enum class TimerType
{
    single_shot,
    Periodic
};

class TimerEvent
{
public:
    TimerEvent(const char *ptr_name, Duration d, TimerType type, TimerCallback cb)
        : m_duration_ms(static_cast<uint32_t>(d.count())), m_type(type), m_callback(std::move(cb)), m_active(true), m_callback_running(false)
    {
        set_name(ptr_name);
        m_next_expiry_ms = someIp::os::now_ms() + m_duration_ms;
    }

    ~TimerEvent()
    {
    }

    TimerEvent(const TimerEvent &) = delete;
    TimerEvent &operator=(const TimerEvent &) = delete;

    void reset(uint32_t duration)
    {
        someIp::os::Guard lock(m_timer_mtx);
        m_active = true;
        m_callback_running = false;
        m_duration_ms = duration;
        m_next_expiry_ms = someIp::os::now_ms() + m_duration_ms;
    }

    void disable()
    {
        someIp::os::Guard lock(m_timer_mtx);
        m_active = false;
        m_callback.reset();
    }

    // {should_fire, callback_to_execute}
    std::pair<bool, TimerCallback> check_expiry()
    {
        someIp::os::Guard lock(m_timer_mtx);
        if (!m_active || m_callback_running)
            return {false, TimerCallback{}};

        // wrap-around safe comparison (sys_now is a u32 ms tick)
        if (static_cast<int32_t>(someIp::os::now_ms() - m_next_expiry_ms) >= 0)
        {
            if (m_callback)
            {
                m_callback_running = true;
                if (m_type == TimerType::single_shot)
                    m_active = false;
                else
                    m_next_expiry_ms = someIp::os::now_ms() + m_duration_ms;
                return {true, m_callback};
            }
            m_active = false;
            return {false, TimerCallback{}};
        }
        return {false, TimerCallback{}};
    }

    void callback_completed()
    {
        someIp::os::Guard lock(m_timer_mtx);
        m_callback_running = false;
    }

    bool get_state()
    {
        someIp::os::Guard lock(m_timer_mtx);
        return m_active;
    }

    bool is_callback_running()
    {
        someIp::os::Guard lock(m_timer_mtx);
        return m_callback_running;
    }

    uint32_t get_duration()
    {
        someIp::os::Guard lock(m_timer_mtx);
        return m_duration_ms;
    }

    const char *get_name() const { return m_name; }

    void set_name(const char *ptr_name)
    {
        std::strncpy(m_name, ptr_name ? ptr_name : "", someIp::config::TIMER_NAME_SIZE - 1);
        m_name[someIp::config::TIMER_NAME_SIZE - 1] = '\0';
    }

    void set_callback(TimerCallback cb)
    {
        someIp::os::Guard lock(m_timer_mtx);
        m_callback = std::move(cb);
    }

private:
    uint32_t m_duration_ms;
    uint32_t m_next_expiry_ms;
    TimerType m_type;
    TimerCallback m_callback;
    bool m_active;
    bool m_callback_running;
    char m_name[someIp::config::TIMER_NAME_SIZE] = {};
    mutable someIp::os::Mutex m_timer_mtx;
};

#endif // SOMEIP_TIMER_HPP_
