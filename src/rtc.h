#pragma once

#include <memory>

class IO;

class RTC final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    RTC(IO& io);
    ~RTC();

    void Reset();
};
