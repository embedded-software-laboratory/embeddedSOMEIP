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

#ifndef SINGLETON_HPP
#define SINGLETON_HPP

#include <new>
#include <type_traits>

#include "someIp/os/Os.hpp"

// thread-safe singleton base, placement-constructed in static storage, no heap
template <typename T>
class Singleton
{
  private:
    struct State {
      std::aligned_storage_t<sizeof(T), alignof(T)> storage;
      T *ptr = nullptr;
      someIp::os::Mutex mtx;
    };
    static State &state() {
      static State s;
      return s;
    }

  public:
    static T *get_instance()
    {
      State &ref_s = state();
      someIp::os::Guard lk(ref_s.mtx);
      if (!ref_s.ptr) {
        ref_s.ptr = ::new (&ref_s.storage) T();
      }
      return ref_s.ptr;
    }

    // destroys the instance in place, next get_instance() rebuilds it
    static void reset_instance()
    {
      State &ref_s = state();
      someIp::os::Guard lk(ref_s.mtx);
      if (ref_s.ptr) {
        ref_s.ptr->~T();
        ref_s.ptr = nullptr;
      }
    }

  protected:
    Singleton() {}
    ~Singleton() {}
  public:
    Singleton(Singleton const &) = delete;
    Singleton& operator=(Singleton const &) = delete;
};

#endif // SINGLETON_HPP
