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

#include <vector>
#include <algorithm>
#include <numeric>
#include <memory>
#include <sstream>
#include <iomanip>

class Statistics{
  public:
    static void sort_vector(std::shared_ptr<std::vector<double>> doubleVector){
      std::sort(doubleVector->begin(), doubleVector->end());
    }

    // calculate a specific percentile
    static double calculate_percentile(std::shared_ptr<std::vector<double>> sorted_times, double percentile) {
        if (sorted_times->empty()) return 0.0;

        size_t index = static_cast<size_t>(percentile * sorted_times->size() / 100.0);
        index = std::min(index, sorted_times->size() - 1); // clamp to range
        return (*sorted_times)[index];
    }

    // Mean
    static double mean (std::shared_ptr<std::vector<double>> sortedTimes){
      return std::accumulate(sortedTimes->begin(), sortedTimes->end(), 0.0) / sortedTimes->size();
    } 

    // Min
    static double min(std::shared_ptr<std::vector<double>> sortedTimes){
      return sortedTimes->front();
    }

    // Max
    static double max (std::shared_ptr<std::vector<double>> sortedTimes){
      return sortedTimes->back();
    };

    // 50%
    static double median (std::shared_ptr<std::vector<double>> sortedTimes){
      return calculate_percentile(sortedTimes, 50);
    };

    // 90%
    static double p90 (std::shared_ptr<std::vector<double>> sortedTimes){
      return calculate_percentile(sortedTimes, 90);
    };

    // 99%
    static double p99 (std::shared_ptr<std::vector<double>> sortedTimes){
      return calculate_percentile(sortedTimes, 99);
    };

    // 99.99%
    static double p9999 (std::shared_ptr<std::vector<double>> sortedTimes){
      return calculate_percentile(sortedTimes, 99.99);
    };

    // returns a printable string with the statistic results
    static std::string statistic_output(std::shared_ptr<std::vector<double>> latencies){
      std::ostringstream resultStream;

      sort_vector(latencies);

      const int COLUMN_WIDTH = 11;
      const int NUMBER_OF_COLUMNS = 7;


      resultStream << std::left; // left-align columns
      resultStream << "Statistics:\n";
      resultStream << std::setw(COLUMN_WIDTH) << "Mean (ms)";
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << "Min (ms)";
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << "Median (ms)";
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << "90% (ms)";
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << "99% (ms)";
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << "99.99% (ms)";
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << "Max (ms)";
      resultStream << "\n";

      for(int column = 0; column < NUMBER_OF_COLUMNS; column++)
      {
        resultStream << std::string(COLUMN_WIDTH, '-'); 
        if(NUMBER_OF_COLUMNS - 1 > column)
        {
          resultStream << "-+-";
        }
      }
      resultStream << "\n";

      resultStream << std::setw(COLUMN_WIDTH) << mean(latencies);
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << min(latencies);
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << median(latencies);
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << p90(latencies);
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << p99(latencies);
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) << p9999(latencies);
      resultStream << " | ";
      resultStream << std::setw(COLUMN_WIDTH) <<  max(latencies);
      resultStream << "\n";

      return resultStream.str();
    };

};
