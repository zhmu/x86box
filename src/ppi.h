#pragma once

#include <memory>

struct IOInterface;
struct PITInterface;

class PPI final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    PPI(IOInterface&, PITInterface&);
    ~PPI();

    void Reset();
};
