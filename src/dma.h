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

    class Transfer
    {
      const int ch_num;
      Impl& impl;

    public:
      Transfer(int ch_num, Impl& impl) : ch_num(ch_num), impl(impl) { }

      size_t WriteFromPeripheral(uint16_t offset, std::span<const uint8_t> data);
      size_t GetTotalLength();
      void Complete();
    };

    void Reset();
    Transfer InitiateTransfer(int ch_num);
};
