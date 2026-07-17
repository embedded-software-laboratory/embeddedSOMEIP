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

#ifndef SOMEIP_UTILS_SESSIONTABLE_HPP
#define SOMEIP_UTILS_SESSIONTABLE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "someIp/utils/CapacityCheck.hpp"

namespace someIp {

// fixed-capacity table, Slot exposes active, matches(Key), set_key(Key)
template <class Slot, size_t N, class Key = uint16_t>
class SessionTable {
public:
  Slot *find(const Key &ref_k) {
    for (auto &ref_s : m_slots)
      if (ref_s.active && ref_s.matches(ref_k)) return &ref_s;
    return nullptr;
  }
  const Slot *find(const Key &ref_k) const {
    for (auto &ref_s : m_slots)
      if (ref_s.active && ref_s.matches(ref_k)) return &ref_s;
    return nullptr;
  }

  bool has(const Key &ref_k) const { return find(ref_k) != nullptr; }

  // first free slot, marked active with this key, or nullptr if full
  Slot *acquire_free(const Key &ref_k) {
    for (auto &ref_s : m_slots) {
      if (!ref_s.active) {
        ref_s = Slot{};
        ref_s.active = true;
        ref_s.set_key(ref_k);
        return &ref_s;
      }
    }
    capacity_exhausted("SessionTable");
    return nullptr;
  }

  // existing session for key, else a newly acquired free slot, else nullptr
  Slot *acquire(const Key &ref_k) {
    if (Slot *ptr_s = find(ref_k)) return ptr_s;
    return acquire_free(ref_k);
  }

  bool release(const Key &ref_k) {
    if (Slot *ptr_s = find(ref_k)) {
      *ptr_s = Slot{};
      return true;
    }
    return false;
  }

  void clear_all() {
    for (auto &ref_s : m_slots) ref_s = Slot{};
  }

  size_t active_count() const {
    size_t n = 0;
    for (auto &ref_s : m_slots)
      if (ref_s.active) ++n;
    return n;
  }
  static constexpr size_t capacity() { return N; }

  Slot *begin() { return m_slots.data(); }
  Slot *end() { return m_slots.data() + N; }

private:
  std::array<Slot, N> m_slots{};
};

} // namespace someIp

#endif // SOMEIP_UTILS_SESSIONTABLE_HPP
