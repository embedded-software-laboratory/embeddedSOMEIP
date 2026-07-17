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

#ifndef SOMEIP_UTILS_INPLACEFUNCTION_HPP
#define SOMEIP_UTILS_INPLACEFUNCTION_HPP

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include "someIp/config/StackConfig.hpp"

namespace someIp {

// type-erased callable with inline storage, larger than N fails to compile
template <class Sig, std::size_t N = config::CALLBACK_INPLACE_SIZE>
class InplaceFunction;

template <class R, class... Args, std::size_t N>
class InplaceFunction<R(Args...), N> {
public:
  InplaceFunction() noexcept = default;

  template <class F,
            class D = std::decay_t<F>,
            class = std::enable_if_t<!std::is_same<D, InplaceFunction>::value>>
  InplaceFunction(F &&ref_f) {
    static_assert(sizeof(D) <= N, "callable too large for InplaceFunction buffer (raise CALLBACK_INPLACE_SIZE)");
    static_assert(alignof(D) <= alignof(std::max_align_t), "callable over-aligned for InplaceFunction buffer");
    ::new (static_cast<void *>(&m_storage)) D(std::forward<F>(ref_f));
    m_ptr_invoke = &invoke_impl<D>;
    m_ptr_manage = &manage_impl<D>;
  }

  InplaceFunction(const InplaceFunction &ref_o) {
    if (ref_o.m_ptr_manage) {
      ref_o.m_ptr_manage(Op::Copy, const_cast<void *>(static_cast<const void *>(&ref_o.m_storage)), &m_storage);
      m_ptr_invoke = ref_o.m_ptr_invoke;
      m_ptr_manage = ref_o.m_ptr_manage;
    }
  }

  InplaceFunction(InplaceFunction &&ref_o) noexcept {
    if (ref_o.m_ptr_manage) {
      ref_o.m_ptr_manage(Op::Move, &ref_o.m_storage, &m_storage);
      m_ptr_invoke = ref_o.m_ptr_invoke;
      m_ptr_manage = ref_o.m_ptr_manage;
      ref_o.reset();
    }
  }

  InplaceFunction &operator=(const InplaceFunction &ref_o) {
    if (this != &ref_o) {
      reset();
      if (ref_o.m_ptr_manage) {
        ref_o.m_ptr_manage(Op::Copy, const_cast<void *>(static_cast<const void *>(&ref_o.m_storage)), &m_storage);
        m_ptr_invoke = ref_o.m_ptr_invoke;
        m_ptr_manage = ref_o.m_ptr_manage;
      }
    }
    return *this;
  }

  InplaceFunction &operator=(InplaceFunction &&ref_o) noexcept {
    if (this != &ref_o) {
      reset();
      if (ref_o.m_ptr_manage) {
        ref_o.m_ptr_manage(Op::Move, &ref_o.m_storage, &m_storage);
        m_ptr_invoke = ref_o.m_ptr_invoke;
        m_ptr_manage = ref_o.m_ptr_manage;
        ref_o.reset();
      }
    }
    return *this;
  }

  template <class F, class = std::enable_if_t<!std::is_same<std::decay_t<F>, InplaceFunction>::value>>
  InplaceFunction &operator=(F &&ref_f) {
    *this = InplaceFunction(std::forward<F>(ref_f));
    return *this;
  }

  ~InplaceFunction() { reset(); }

  R operator()(Args... args) const {
    return m_ptr_invoke(const_cast<void *>(static_cast<const void *>(&m_storage)), std::forward<Args>(args)...);
  }

  explicit operator bool() const noexcept { return m_ptr_invoke != nullptr; }

  void reset() noexcept {
    if (m_ptr_manage) {
      m_ptr_manage(Op::Destroy, &m_storage, nullptr);
      m_ptr_invoke = nullptr;
      m_ptr_manage = nullptr;
    }
  }

private:
  enum class Op { Copy, Move, Destroy };

  template <class D>
  static R invoke_impl(void *ptr_s, Args... args) {
    return (*static_cast<D *>(ptr_s))(std::forward<Args>(args)...);
  }

  template <class D>
  static void manage_impl(Op op, void *ptr_src, void *ptr_dst) {
    switch (op) {
      case Op::Copy:
        ::new (ptr_dst) D(*static_cast<D *>(ptr_src));
        break;
      case Op::Move:
        ::new (ptr_dst) D(std::move(*static_cast<D *>(ptr_src)));
        break;
      case Op::Destroy:
        static_cast<D *>(ptr_src)->~D();
        break;
    }
  }

  using Storage = std::aligned_storage_t<N, alignof(std::max_align_t)>;
  Storage m_storage;
  R (*m_ptr_invoke)(void *, Args...) = nullptr;
  void (*m_ptr_manage)(Op, void *, void *) = nullptr;
};

} // namespace someIp

#endif // SOMEIP_UTILS_INPLACEFUNCTION_HPP
