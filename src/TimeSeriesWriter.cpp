#include <cstring>  // for memcpy
#include <cstdio>   // for FILE*, fopen, fwrite, fflush, fclose
#if defined(__cplusplus) && (__cplusplus >= 201703L)
#include <filesystem>
#endif
#include <stdexcept>

#include "TimeSeriesWriter.h"

void ensureDirectoryExists(const std::string& filePath) {
#if defined(__cplusplus) && (__cplusplus >= 201703L)
    std::filesystem::path path(filePath);
    auto dir = path.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir)) {
        bool created = std::filesystem::create_directories(dir);
        if (!created) {
            throw std::runtime_error("Failed to create directory: " + dir.string());
        }
    }
#endif
}

TimeSeriesWriter::TimeSeriesWriter(const std::string& binPath,
                                   std::unique_ptr<TimeSeriesType> data,
                                   size_t chunkSize)
    : binPath_(binPath), data_(std::move(data)), chunkSize_(chunkSize), binFile_(nullptr) {
    ensureDirectoryExists(binPath_);

    binFile_ = fopen(binPath_.c_str(), "wb");
    if (!binFile_) {
        throw std::runtime_error("Failed to open binary file for writing: " + binPath_);
    }

    // 写入头长度和头数据
    uint32_t len = data_->head_array_length();
    if (fwrite(&len, sizeof(uint32_t), 1, binFile_) != 1) {
        fclose(binFile_);
        binFile_ = nullptr;
        throw std::runtime_error("Failed to write header length to file: " + binPath_);
    }

    const auto& headArray = data_->head_array();
    if (!headArray.empty()) {
        if (fwrite(headArray.data(), sizeof(uint8_t), len, binFile_) != len) {
            fclose(binFile_);
            binFile_ = nullptr;
            throw std::runtime_error("Failed to write header data to file: " + binPath_);
        }
    }

    // 初始化缓冲区
    buffer_.resize(chunkSize_ * data_->size());
    bufferIndex_ = 0;
}

TimeSeriesWriter::~TimeSeriesWriter() {
    flush(); // 确保缓冲区数据写出
    if (binFile_) {
        fclose(binFile_);
        binFile_ = nullptr;
    }
}

void TimeSeriesWriter::save(const TimeSeriesType& data) {
    std::vector<uint8_t> buf = data.serialize();
    save(buf.data());
}

void TimeSeriesWriter::save(const void* ptr) {
    size_t itemSize = data_->size();
    std::memcpy(buffer_.data() + bufferIndex_ * itemSize, ptr, itemSize);
    flush();
}

void TimeSeriesWriter::save(const double& timestamp, const void* ptr) {
    size_t itemSize = data_->size();
    std::memcpy(buffer_.data() + bufferIndex_ * itemSize, &timestamp, sizeof(timestamp));
    std::memcpy(buffer_.data() + bufferIndex_ * itemSize + sizeof(timestamp), ptr, itemSize - sizeof(timestamp));
    flush();
}

void TimeSeriesWriter::flush() {
    bufferIndex_++;
    // 缓冲区未满，不写入磁盘
    if (bufferIndex_ < chunkSize_) return;

    if (!binFile_) throw std::runtime_error("Binary file is not open: " + binPath_);

    size_t totalBytes = bufferIndex_ * data_->size();
    if (fwrite(buffer_.data(), 1, totalBytes, binFile_) != totalBytes) {
        throw std::runtime_error("Failed to write data to file during flush: " + binPath_);
    }

    // 成功写入后清空缓冲区索引
    bufferIndex_ = 0;

    // 可选：强制刷新到磁盘
    if (fflush(binFile_) != 0) {
        throw std::runtime_error("Failed to flush data to disk: " + binPath_);
    }
}
