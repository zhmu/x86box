#pragma once

#include <memory>
#include "../interface/dmainterface.h"

struct IOInterface;
struct MemoryInterface;

class DMA final : public DMAInterface
{
public:
    struct Impl;

private:
    std::unique_ptr<Impl> impl;

  public:
    DMA(IOInterface&, MemoryInterface&);
    ~DMA();

    void Reset();
    std::unique_ptr<DMATransfer> InitiateTransfer(int ch_num) override;
};
