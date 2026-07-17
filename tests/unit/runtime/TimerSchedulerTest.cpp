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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "someIp/utils/TimerScheduler.hpp"

namespace {
void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
} // namespace

TEST(EventManagerTest, PeriodicTimerFiresRepeatedlyThenCancels) {
  TimerScheduler em;
  em.start();

  std::atomic<int> ticks{0};
  auto timer = em.add_timer("periodic", 10, [&ticks] { ticks++; }, true);

  for (int i = 0; i < 200 && ticks.load() < 3; ++i) sleep_ms(2);
  EXPECT_GE(ticks.load(), 3);

  em.disable_timer(timer);
  int after = ticks.load();
  sleep_ms(100); // ~10 intervals
  EXPECT_LE(ticks.load() - after, 1) << "timer kept firing after disable_timer";

  em.stop();
}

TEST(EventManagerTest, SingleShotFiresOnce) {
  TimerScheduler em;
  em.start();

  std::atomic<int> ticks{0};
  em.add_timer("oneshot", 10, [&ticks] { ticks++; }, false);

  sleep_ms(120);
  EXPECT_EQ(ticks.load(), 1);

  em.stop();
}
