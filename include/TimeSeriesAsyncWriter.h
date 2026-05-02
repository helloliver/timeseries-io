#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>
#include <memory>
#include <cmath>

#include "TimeSeriesType.h"

class TimeSeriesAsyncWriter {
public:
    TimeSeriesAsyncWriter(const std::string& binPath,
                          std::unique_ptr<TimeSeriesType> dataType,
                          size_t chunkSize = 128);

    ~TimeSeriesAsyncWriter();

    void save(const TimeSeriesType& data);
    void save(const void* buffer);
    void save(double timestamp, const void* buffer);

    void set_timestamp(double timestamp);
    double get_timestamp(void);

private:
    void writerThreadLoop();
    void enqueue(const uint8_t* data, size_t size);

private:
    std::ofstream binFile_;
    std::string binPath_;
    std::unique_ptr<TimeSeriesType> data_;
    double timestamp_ = NAN;

    size_t dataSize_;
    size_t chunkSize_;

    // 后台队列
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::vector<uint8_t>> tasks_;

    std::atomic<bool> running_{true};
    std::thread writerThread_;
};
