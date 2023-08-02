#pragma once

#include <memory>
#include "dmainterface.h"

struct IOInterface;
class Memory;

class DMA final : public DMAInterface
{
public:
    struct Impl;

private:
    std::unique_ptr<Impl> impl;

  public:
    DMA(IOInterface&, Memory&);
    ~DMA();

    void Reset();
    std::unique_ptr<DMATransfer> InitiateTransfer(int ch_num) override;
};
