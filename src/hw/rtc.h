#pragma once

#include <memory>

struct IOInterface;

class RTC final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    RTC(IOInterface& io);
    ~RTC();

    void Reset();
};
