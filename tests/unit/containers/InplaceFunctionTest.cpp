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

#include "someIp/utils/InplaceFunction.hpp"

using someIp::InplaceFunction;

TEST(InplaceFunctionTest, EmptyByDefault) {
  InplaceFunction<int(int)> f;
  EXPECT_FALSE(static_cast<bool>(f));
}

TEST(InplaceFunctionTest, InvokesAndCapturesState) {
  int base = 10;
  InplaceFunction<int(int)> f = [base](int x) { return base + x; };
  ASSERT_TRUE(static_cast<bool>(f));
  EXPECT_EQ(f(5), 15);
}

TEST(InplaceFunctionTest, CopyIsIndependent) {
  InplaceFunction<int(int)> a = [](int x) { return x * 2; };
  InplaceFunction<int(int)> b = a;
  EXPECT_EQ(a(3), 6);
  EXPECT_EQ(b(3), 6);
}

TEST(InplaceFunctionTest, MoveLeavesSourceEmpty) {
  InplaceFunction<int(int)> a = [](int x) { return x + 1; };
  InplaceFunction<int(int)> b = std::move(a);
  EXPECT_TRUE(static_cast<bool>(b));
  EXPECT_EQ(b(1), 2);
  EXPECT_FALSE(static_cast<bool>(a));
}

TEST(InplaceFunctionTest, ResetMakesEmpty) {
  InplaceFunction<int(int)> f = [](int x) { return x; };
  f.reset();
  EXPECT_FALSE(static_cast<bool>(f));
}

TEST(InplaceFunctionTest, ReassignReplacesCallable) {
  InplaceFunction<int(int)> f = [](int x) { return x; };
  f = [](int x) { return -x; };
  EXPECT_EQ(f(7), -7);
}

// mutating state through a captured pointer survives across calls
TEST(InplaceFunctionTest, SideEffectsPersist) {
  int counter = 0;
  InplaceFunction<void()> f = [&counter] { counter++; };
  f();
  f();
  EXPECT_EQ(counter, 2);
}
