#pragma once

#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cstring>

#include "TimeSeriesType.h"
#include "TimeSeriesWriter.h"

class TimeSeriesReader {
   private:
    std::unique_ptr<TimeSeriesType> data_;

   public:
    TimeSeriesType& data() { return *data_; }
    const TimeSeriesType& data() const { return *data_; }

   private:
    std::string csvPath_;
    std::string binPath_;
    std::ifstream binFile_;

    // Streaming read buffer
    std::vector<uint8_t> buffer_;
    size_t chunkSize_;
    size_t bufferIndex_ = 0;

    // Total number of data points
    size_t length_ = 0;
    // Current read position
    size_t position_ = 0;

    double timestamp_ = -std::numeric_limits<double>::max();

   public:
    TimeSeriesReader(const std::string& csvPath,
                     const std::string& binPath,
                     std::unique_ptr<TimeSeriesType> data,
                     size_t chunkSize = 100000);

    TimeSeriesReader(const std::string& csvPath,
                     std::unique_ptr<TimeSeriesType> data,
                     size_t chunkSize = 100000);

    ~TimeSeriesReader();

    // Get the next data
    bool next(void);

    // Reset read position to the beginning
    void rewind(void);

    // Get the first data
    std::unique_ptr<TimeSeriesType> first(void);

    // Get the last data
    std::unique_ptr<TimeSeriesType> last(void);

    // Get total number of datas
    size_t size(void) { return length_; }

    // Get current read position
    size_t position(void) { return position_; }

   private:
    // Load the next chunk of data into the buffer
    void load(void);
};
