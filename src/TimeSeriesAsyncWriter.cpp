#include <stdexcept>

#if defined(__cplusplus) && (__cplusplus >= 201703L)
#include <filesystem>
#endif

#include "TimeSeriesAsyncWriter.h"

static void ensureDirectoryExists(const std::string& filePath) {
#if __cplusplus >= 201703L
    std::filesystem::path path(filePath);
    auto dir = path.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        if (!std::filesystem::create_directories(dir)) {
            throw std::runtime_error("Failed to create directory: " + dir.string());
        }
    }
#endif
}

TimeSeriesAsyncWriter::TimeSeriesAsyncWriter(const std::string& binPath,
                                             std::unique_ptr<TimeSeriesType> dataType,
                                             size_t chunkSize)
    : binPath_(binPath),
      data_(std::move(dataType)),
      chunkSize_(chunkSize)
{
    ensureDirectoryExists(binPath_);

    binFile_.open(binPath_, std::ios::binary);
    if (!binFile_) {
        throw std::runtime_error("Failed to open binary file for writing: " + binPath_);
    }

    // 写入头数据
    uint32_t len = data_->head_array_length();
    binFile_.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    binFile_.write(reinterpret_cast<const char*>(data_->head_array().data()), len);
    if (!binFile_) {
        throw std::runtime_error("Failed to write header to: " + binPath_);
    }

    dataSize_ = data_->size();
    if (dataSize_ == 0)
        throw std::runtime_error("Data size cannot be zero.");

    // 启动后台线程
    writerThread_ = std::thread(&TimeSeriesAsyncWriter::writerThreadLoop, this);
}

TimeSeriesAsyncWriter::~TimeSeriesAsyncWriter() {
    running_ = false;
    cv_.notify_all();

    if (writerThread_.joinable())
        writerThread_.join();

    // 所有任务已处理完，可以安全关闭
    if (binFile_.is_open())
        binFile_.close();
}

void TimeSeriesAsyncWriter::enqueue(const uint8_t* data, size_t size) {
    std::vector<uint8_t> buf(data, data + size);
    {
        std::lock_guard<std::mutex> lock(mtx_);
        tasks_.push(std::move(buf));
    }
    cv_.notify_one();
}

void TimeSeriesAsyncWriter::save(const TimeSeriesType& data) {
    auto buf = data.serialize();
    enqueue(buf.data(), buf.size());
}

void TimeSeriesAsyncWriter::save(const void* buffer) {
    enqueue(reinterpret_cast<const uint8_t*>(buffer), dataSize_);
}

void TimeSeriesAsyncWriter::save(double timestamp, const void* buffer) {
    std::vector<uint8_t> tmp(dataSize_);
    memcpy(tmp.data(), &timestamp, sizeof(timestamp));
    memcpy(tmp.data() + sizeof(timestamp), buffer, dataSize_ - sizeof(timestamp));
    enqueue(tmp.data(), tmp.size());
}

void TimeSeriesAsyncWriter::writerThreadLoop() {
    std::vector<uint8_t> chunk;
    chunk.reserve(chunkSize_ * dataSize_);

    while (running_) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&] { return !tasks_.empty() || !running_; });

        if (!running_ && tasks_.empty())
            break;

        // 一次最多取 chunkSize_ 个，减少磁盘 IO 次数
        for (size_t i = 0; i < chunkSize_ && !tasks_.empty(); ++i) {
            auto& front = tasks_.front();
            chunk.insert(chunk.end(), front.begin(), front.end());
            tasks_.pop();
        }
        lock.unlock();

        // 写入磁盘
        binFile_.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
        if (!binFile_) {
            throw std::runtime_error("Write failed for file: " + binPath_);
        }

        chunk.clear();
    }

    // 离开前写完剩余数据
    while (true) {
        std::vector<uint8_t> chunk2;
        chunk2.reserve(chunkSize_ * dataSize_);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (tasks_.empty()) break;

            while (!tasks_.empty()) {
                auto& front = tasks_.front();
                chunk2.insert(chunk2.end(), front.begin(), front.end());
                tasks_.pop();
            }
        }

        binFile_.write(reinterpret_cast<const char*>(chunk2.data()), chunk2.size());
        if (!binFile_) {
            throw std::runtime_error("Final write failed for file: " + binPath_);
        }
    }
}
