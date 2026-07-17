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

// completion decided by byte coverage not a running byte sum
#include <gtest/gtest.h>

#include "someIp/transport-protocol/Reassembler.hpp"

using someIp::tp::RxSession;

TEST(ReassemblyCoverage, ContiguousIsComplete) {
  RxSession s;
  s.mark_covered(0, 512);
  s.mark_covered(512, 512);
  s.mark_covered(1024, 200); // final non-16-multiple length
  EXPECT_TRUE(s.fully_covered(1224));
}

TEST(ReassemblyCoverage, HoleIsNotComplete) {
  RxSession s;
  s.mark_covered(0, 512);
  // skip [512, 1024)
  s.mark_covered(1024, 200);
  EXPECT_FALSE(s.fully_covered(1224));
}

// duplicated first segment must not mask a missing middle segment
TEST(ReassemblyCoverage, DuplicatesDoNotMaskHole) {
  RxSession s;
  s.mark_covered(0, 512);
  s.mark_covered(0, 512);   // duplicate
  s.mark_covered(1024, 200);
  EXPECT_FALSE(s.fully_covered(1224)); // [512,1024) still missing
}

TEST(ReassemblyCoverage, OverlapDoesNotMaskHole) {
  RxSession s;
  s.mark_covered(0, 768);
  s.mark_covered(256, 512); // overlaps [256,768)
  // [768, 1024) missing
  s.mark_covered(1024, 200);
  EXPECT_FALSE(s.fully_covered(1224));
}

TEST(ReassemblyCoverage, ClearResetsCoverage) {
  RxSession s;
  s.mark_covered(0, 1024);
  EXPECT_TRUE(s.fully_covered(1024));
  s.clear_coverage();
  EXPECT_FALSE(s.fully_covered(16));
}
