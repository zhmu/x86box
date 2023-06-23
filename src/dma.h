#pragma once

#include <memory>

class IO;

class DMA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    DMA(IO& io);
    ~DMA();

    void Reset();
};
