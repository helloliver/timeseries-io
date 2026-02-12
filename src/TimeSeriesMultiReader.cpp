#include <limits>
#include <stdexcept>
#include "TimeSeriesMultiReader.h"

TimeSeriesMultiReader::Source::Source(std::unique_ptr<TimeSeriesReader> r, double delay)
    : reader(std::move(r)), delay(delay) {
    next();
}

bool TimeSeriesMultiReader::Source::next(void) {
    is_valid = reader->next();
    return is_valid;
}

double TimeSeriesMultiReader::Source::time_start(void) {
    auto first = reader->first();
    if (first) {
        return first->get_timestamp();
    }
    return std::numeric_limits<double>::max();
}

double TimeSeriesMultiReader::Source::time_end(void) {
    auto last = reader->last();
    if (last) {
        return last->get_timestamp();
    }
    return -std::numeric_limits<double>::max();
}

void TimeSeriesMultiReader::add(const std::string& csv_path,
                                const std::string& bin_path,
                                std::unique_ptr<TimeSeriesType> descriptor,
                                size_t chunk_size,
                                const std::vector<double>& block_time_list,
                                double delay) {
    if (sources_lock_) throw std::runtime_error("TimeSeriesMultiReader: sources have been locked");
    auto r = std::make_unique<TimeSeriesReader>(csv_path, bin_path, std::move(descriptor), chunk_size, block_time_list);
    sources_.emplace_back(std::make_unique<Source>(std::move(r), delay));
    min_idx_ = sources_.size();
}

void TimeSeriesMultiReader::add(const std::string& csv_path,
                                std::unique_ptr<TimeSeriesType> descriptor,
                                size_t chunk_size,
                                const std::vector<double>& block_time_list,
                                double delay) {
    if (sources_lock_) throw std::runtime_error("TimeSeriesMultiReader: sources have been locked");
    auto r = std::make_unique<TimeSeriesReader>(csv_path, std::move(descriptor), chunk_size, block_time_list);
    sources_.emplace_back(std::make_unique<Source>(std::move(r), delay));
    min_idx_ = sources_.size();
}

void TimeSeriesMultiReader::add(std::unique_ptr<TimeSeriesReader> r, double delay) {
    if (sources_lock_) throw std::runtime_error("TimeSeriesMultiReader: sources have been locked");
    sources_.emplace_back(std::make_unique<Source>(std::move(r), delay));
    min_idx_ = sources_.size();
}

std::unique_ptr<TimeSeriesType> TimeSeriesMultiReader::next(double& timestamp) {
    sources_lock_ = true;

    if (min_idx_ != sources_.size()) {
        sources_[min_idx_]->next();
        min_idx_ = sources_.size();
    }

    double min_timestamp = std::numeric_limits<double>::max();
    for (size_t i = 0; i < sources_.size(); ++i) {
        if (sources_[i]->is_valid &&
            sources_[i]->reader->data().get_timestamp() + sources_[i]->delay < min_timestamp) {
            min_timestamp = sources_[i]->reader->data().get_timestamp() + sources_[i]->delay;
            min_idx_ = i;
        }
    }

    if (min_idx_ == sources_.size()) {
        return nullptr;
    }

    auto s = sources_[min_idx_].get();
    timestamp = s->reader->data().get_timestamp() + s->delay;

    return s->reader->data().clone();
}

std::unique_ptr<TimeSeriesType> TimeSeriesMultiReader::next(void) {
    double timestamp;
    return next(timestamp);
}

void TimeSeriesMultiReader::rewind(void) {
    for (auto& s : sources_) {
        s->reader->rewind();
        s->next();
    }
    min_idx_ = sources_.size();
    sources_lock_ = false;
}

double TimeSeriesMultiReader::time_start(void) {
    double min_timestamp = std::numeric_limits<double>::max();
    for (auto& s : sources_) {
        min_timestamp = std::min<double>(min_timestamp, s->time_start());
    }
    return min_timestamp;
}

double TimeSeriesMultiReader::time_end(void) {
    double max_timestamp = -std::numeric_limits<double>::max();
    for (auto& s : sources_) {
        max_timestamp = std::max<double>(max_timestamp, s->time_end());
    }
    return max_timestamp;
}
