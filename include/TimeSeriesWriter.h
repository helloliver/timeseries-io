#pragma once

#include <fstream>
#include <vector>
#include <string>
#include <memory>

#include "TimeSeriesType.h"

class TimeSeriesWriter {
private:
    std::string binPath_;
    std::ofstream binFile_;
    std::unique_ptr<TimeSeriesType> data_;

    size_t chunkSize_;
    std::vector<uint8_t> buffer_;
    size_t bufferIndex_ = 0;

public:
    TimeSeriesWriter(const std::string& binPath,
                     std::unique_ptr<TimeSeriesType> data,
                     size_t chunkSize = 1);
    ~TimeSeriesWriter();

    void save(const TimeSeriesType& data);
    void save(const void* data);
    void save(const double& timestamp, const void* ptr);

    void flush(void);
};
