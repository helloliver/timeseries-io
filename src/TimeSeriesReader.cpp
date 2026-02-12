#include <sys/stat.h>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <iostream>
#include "TimeSeriesReader.h"

static bool checkFile(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}

static std::time_t getModTime(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 ? info.st_mtime : 0;
}

static std::string convertBinaryPath(const std::string& csvPath) {
    auto dotPos = csvPath.find_last_of('.');
    if (dotPos == std::string::npos) throw std::runtime_error("Invalid CSV path");
    return csvPath.substr(0, dotPos) + ".bin";
}

static void convertCSVToBinary(const std::string& csvPath, const std::string& binPath, TimeSeriesType& data, size_t chunkSize) {
    std::ifstream csvFile(csvPath);
    if (!csvFile) throw std::runtime_error("Failed to open CSV file");

    TimeSeriesWriter writer(binPath, data.clone(), chunkSize);

    std::string line;
    while (std::getline(csvFile, line)) {
        if (line.empty()) continue;
        if (data.sscan(line.c_str()))
            writer.save(data.serialize().data());
    }
}

TimeSeriesReader::TimeSeriesReader(const std::string& csvPath,
                                   const std::string& binPath,
                                   std::unique_ptr<TimeSeriesType> data,
                                   size_t chunkSize,
                                   const std::vector<double>& block_time_list)
    : csvPath_(csvPath), binPath_(binPath), data_(std::move(data)), chunkSize_(chunkSize) {
    if (!data_) {
        valid_ = false;
        std::cout << "[TimeSeriesReader] std::unique_ptr<TimeSeriesType> data is null" << std::endl;
        return;
    }

    bool needConvert = checkFile(csvPath_) && (!checkFile(binPath_) || getModTime(csvPath_) > getModTime(binPath_));
    if (needConvert) convertCSVToBinary(csvPath_, binPath_, *data_, chunkSize_);

    binFile_.open(binPath_, std::ios::binary);
    if (!binFile_) {
        valid_ = false;
        std::cout << "[TimeSeriesReader] Failed to open binary file :" << binPath_ << std::endl;
        return;
    }

    uint32_t len = 0;
    binFile_.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (len != data_->head_array_length()) {
        valid_ = false;
        std::cout << "[TimeSeriesReader] Binary header length mismatch" << std::endl;
        return;
    }

    std::vector<uint8_t> head(len);
    binFile_.read(reinterpret_cast<char*>(head.data()), len);
    if (memcmp(head.data(), data_->head_array().data(), len) != 0) {
        valid_ = false;
        std::cout << "[TimeSeriesReader] Binary header content mismatch" << std::endl;
        return;
    }

    binFile_.seekg(0, std::ios::end);
    size_t totalBytes = static_cast<size_t>(binFile_.tellg()) - sizeof(uint32_t) - len;
    length_ = totalBytes / data_->size();
    binFile_.seekg(sizeof(uint32_t) + len, std::ios::beg);

    buffer_.resize(chunkSize_ * data_->size());
    load();

    initSkipRanges(block_time_list);
}

TimeSeriesReader::TimeSeriesReader(const std::string& csvPath,
                                   std::unique_ptr<TimeSeriesType> data,
                                   size_t chunkSize,
                                   const std::vector<double>& block_time_list)
    : TimeSeriesReader(csvPath, convertBinaryPath(csvPath), std::move(data), chunkSize, block_time_list) {
}

TimeSeriesReader::~TimeSeriesReader() {
    if (binFile_.is_open()) binFile_.close();
}

void TimeSeriesReader::initSkipRanges(const std::vector<double>& block_time_list) {
    if (block_time_list.size() % 2 != 0) {
        std::cout << "[TimeSeriesReader] Warning: block_time_list size is not even, ignoring skip ranges" << std::endl;
        return;
    }

    for (size_t i = 0; i < block_time_list.size(); i += 2) {
        double start = block_time_list[i];
        double end = block_time_list[i + 1];
        if (start < end) {
            skipRanges_.emplace_back(start, end);
        } else {
            std::cout << "[TimeSeriesReader] Warning: invalid skip range [" 
                      << start << ", " << end << "], start >= end" << std::endl;
        }
    }
}

bool TimeSeriesReader::shouldSkip(double timestamp) const {
    for (const auto& range : skipRanges_) {
        if (timestamp >= range.first && timestamp <= range.second) {
            return true;
        }
    }
    return false;
}

void TimeSeriesReader::load(void) {
    size_t remain = length_ - position_;
    size_t toRead = std::min(chunkSize_, remain);
    binFile_.read(reinterpret_cast<char*>(buffer_.data()), toRead * data_->size());
    bufferIndex_ = 0;
}

bool TimeSeriesReader::next(void) {
    if (!valid_) return false;
    if (position_ >= length_) return false;

    while (true) {
        if (position_ >= length_) return false;

        if (bufferIndex_ >= buffer_.size() / data_->size()) {
            load();
        }

        bool res = data_->deserialize(buffer_.data() + bufferIndex_ * data_->size());
        bufferIndex_++;
        position_++;

        timestamp_ = data_->get_timestamp();

        // Skip if timestamp is in any skip range
        if (skipRanges_.empty() || !shouldSkip(timestamp_)) {
            return res;
        }

        // Otherwise continue to next data point
    }
}

void TimeSeriesReader::rewind(void) {
    binFile_.clear();
    binFile_.seekg(sizeof(uint32_t) + data_->head_array_length(), std::ios::beg);
    position_ = 0;
    timestamp_ = -std::numeric_limits<double>::max();
    load();
}

std::unique_ptr<TimeSeriesType> TimeSeriesReader::first(void) {
    if (!valid_) return nullptr;
    if (length_ == 0) return nullptr;

    std::ifstream file(binPath_, std::ios::binary);
    if (!file) return nullptr;

    file.seekg(sizeof(uint32_t) + data_->head_array_length(), std::ios::beg);
    std::vector<uint8_t> buffer(data_->size());
    file.read(reinterpret_cast<char*>(buffer.data()), data_->size());

    std::unique_ptr<TimeSeriesType> type_clone = data_->clone();
    if (file.good() && type_clone->deserialize(buffer.data())) {
        return type_clone;
    } else {
        return nullptr;
    }
}

std::unique_ptr<TimeSeriesType> TimeSeriesReader::last(void) {
    if (!valid_) return nullptr;
    if (length_ == 0) return nullptr;

    std::ifstream file(binPath_, std::ios::binary);
    if (!file) return nullptr;

    file.seekg(sizeof(uint32_t) + data_->head_array_length() + (length_ - 1) * data_->size(), std::ios::beg);
    std::vector<uint8_t> buffer(data_->size());
    file.read(reinterpret_cast<char*>(buffer.data()), data_->size());

    std::unique_ptr<TimeSeriesType> type_clone = data_->clone();
    if (file.good() && type_clone->deserialize(buffer.data())) {
        return type_clone;
    } else {
        return nullptr;
    }
}
