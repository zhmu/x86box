#pragma once

#include <memory>
#include <span>

class IO;
class Memory;

class DMA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    DMA(IO&, Memory&);
    ~DMA();

    void Reset();
    void WriteDataFromPeriphal(int ch, std::span<const uint8_t> data);
};
