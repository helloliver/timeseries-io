#pragma once

#include <stdint.h>
#include <vector>
#include <memory>
#include <string>

class TimeSeriesType {
   private:
    std::string label_;

   public:
    TimeSeriesType() : label_("") {}

    TimeSeriesType(std::string label) : label_(label) {}

    TimeSeriesType(const char* label) : label_(label) {}

    virtual ~TimeSeriesType() = default;

    virtual double get_timestamp(void) const = 0;

    const std::string get_label(void) const { return label_; }

    virtual std::vector<uint8_t> serialize(void) const = 0;

    virtual bool deserialize(const std::vector<uint8_t>& buffer) = 0;

    virtual bool deserialize(const void* buffer) = 0;

    virtual size_t size(void) const = 0;

    virtual const std::vector<uint8_t> head_array(void) const = 0;

    virtual uint32_t head_array_length(void) const = 0;

    virtual bool sscan(const char *__source) = 0;

    virtual int print(void) const = 0;

    virtual int fprint(FILE *__stream) const = 0;

    virtual void rnd(void) = 0;

    virtual std::unique_ptr<TimeSeriesType> clone(void) const = 0;
};
