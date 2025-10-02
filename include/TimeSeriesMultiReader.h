#pragma once

#include <vector>
#include <memory>
#include <string>

#include "TimeSeriesType.h"
#include "TimeSeriesReader.h"

class TimeSeriesMultiReader {
   private:
    struct Source {
        std::unique_ptr<TimeSeriesReader> reader;
        double delay = 0;
        bool is_valid = false;

        Source(std::unique_ptr<TimeSeriesReader> r, double delay = 0);

        ~Source() {};

        bool next(void);

        double time_start(void);

        double time_end(void);
    };

    std::vector<std::unique_ptr<Source>> sources_;
    size_t min_idx_ = 0;
    bool sources_lock_ = false;

   public:
    TimeSeriesMultiReader() : min_idx_(0), sources_lock_(false) {}

    void add(const std::string& csv_path,
             const std::string& bin_path,
             std::unique_ptr<TimeSeriesType> descriptor,
             size_t chunk_size = 100000,
             double delay = 0);

    void add(const std::string& csv_path,
             std::unique_ptr<TimeSeriesType> descriptor,
             size_t chunk_size = 100000,
             double delay = 0);

    void add(std::unique_ptr<TimeSeriesReader> r, double delay = 0);

    std::unique_ptr<TimeSeriesType> next(double& timestamp);

    std::unique_ptr<TimeSeriesType> next(void);

    void rewind(void);

    double time_start(void);

    double time_end(void);
};
