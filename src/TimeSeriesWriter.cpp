#include <stdexcept>

#if defined(__cplusplus) && (__cplusplus >= 201703L)
#include <filesystem>
#endif

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
    : binPath_(binPath), data_(std::move(data)), chunkSize_(chunkSize) {
    ensureDirectoryExists(binPath_);
    binFile_.open(binPath_, std::ios::binary);
    if (!binFile_) throw std::runtime_error("Failed to open binary file for writing: " + binPath_);

    uint32_t len = data_->head_array_length();
    binFile_.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    binFile_.write(reinterpret_cast<const char*>(data_->head_array().data()), len);

    buffer_.resize(chunkSize_ * data_->size());
}

TimeSeriesWriter::~TimeSeriesWriter() {
    flush();
    if (binFile_.is_open()) binFile_.close();
}

void TimeSeriesWriter::save(const TimeSeriesType& data) {
    std::vector<uint8_t> buf = data.serialize();
    save(buf.data());
    set_timestamp(data.get_timestamp());
}

void TimeSeriesWriter::save(const void* buffer) {
    memcpy(buffer_.data() + bufferIndex_ * data_->size(), buffer, data_->size());
    bufferIndex_++;
    if (bufferIndex_ >= chunkSize_) flush();
}

void TimeSeriesWriter::save(const double& timestamp, const void* buffer) {
    memcpy(buffer_.data() + bufferIndex_ * data_->size(), &timestamp, sizeof(timestamp));
    memcpy(buffer_.data() + bufferIndex_ * data_->size() + sizeof(timestamp), buffer, data_->size() - sizeof(timestamp));
    bufferIndex_++;
    if (bufferIndex_ >= chunkSize_) flush();
    set_timestamp(timestamp);
}

void TimeSeriesWriter::set_timestamp(double timestamp) {
    timestamp_ = timestamp;
}

double TimeSeriesWriter::get_timestamp(void) {
    return timestamp_;
}

void TimeSeriesWriter::flush(void) {
    if (bufferIndex_ == 0) return;
    binFile_.write(reinterpret_cast<const char*>(buffer_.data()), bufferIndex_ * data_->size());
    bufferIndex_ = 0;
}
