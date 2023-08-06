#pragma once

#include <memory>

struct IOInterface;
struct TimeInterface;

class RTC final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    RTC(IOInterface& io, TimeInterface& time);
    ~RTC();

    void Reset();
};
