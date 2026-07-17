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

#include <cstdio>

// console progress bar for long tests
class ProgressBar {
  public:
    ProgressBar(int numberOfRuns, std::shared_ptr<SuppressOutput> suppress) : NUMBER_OF_RUNS(numberOfRuns), suppress(suppress) {};
    ~ProgressBar() = default;

    void show(int currentRun){
      int step = NUMBER_OF_RUNS / 100;
      if (step == 0) step = 1;  // ensure non-zero

      if(currentRun % step == 0) {
        int progress = (currentRun * 100) / NUMBER_OF_RUNS;
        int pos = BAR_WIDTH * progress / 100;
        suppress->PrintOnce([this, progress, pos]{
          printf("Progress: [");
          for (int j = 0; j < BAR_WIDTH; ++j) {
            if (j < pos) printf("#");
            else printf("-");
          }
          printf("] %3d%%\r", progress);
          fflush(stdout);
        });
      }
    }

    void complete() {
      suppress->PrintOnce([this]{
        printf("Progress: [");
        for (int j = 0; j < BAR_WIDTH; ++j) {
          printf("#");
        }
        printf("] 100%%\n");
        fflush(stdout);
      });
    }

  private:
    const int BAR_WIDTH = 50;
    const int NUMBER_OF_RUNS;
    std::shared_ptr<SuppressOutput> suppress;
};
