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
#include <cstdio>

// suppresses console output during unittesting
class SuppressOutput {
public:
  SuppressOutput() {
#ifdef _WIN32
    null_device = fopen("nul", "w");
#else
    null_device = fopen("/dev/null", "w");
#endif
    if (null_device) {
      original_stdout = stdout;
      stdout = null_device;
    }
  }

  ~SuppressOutput() {
      if (null_device) {
        fflush(stdout);
        stdout = original_stdout;
        fclose(null_device);
      }
  }

  // restores normal console output
  void AllowOutput() {
    if (null_device) {
      fflush(stdout);
      stdout = original_stdout;
    }
    std::cout.rdbuf(original_cout);
  }

  // prints a single message while suppressing the rest
  template <typename Callable>
  void PrintOnce(Callable print_action) {
    if (null_device) {
      fflush(stdout);
      stdout = original_stdout;
    }
    std::cout.rdbuf(original_cout);

    print_action();

    if (null_device) {
      fflush(stdout);
      stdout = null_device;
    }
    std::cout.rdbuf(null_stream.rdbuf());
  }

private:
  FILE* null_device = nullptr;
  FILE* original_stdout = nullptr;
  std::streambuf* original_cout = nullptr;
  std::ostringstream null_stream;
};