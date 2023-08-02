#pragma once

#include <memory>
#include "../interface/picinterface.h"

struct IOInterface;

class PIC final : public PICInterface
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    PIC(IOInterface& io);
    ~PIC();

    void AssertIRQ(IRQ irq) override;
    void SetPendingIRQState(IRQ irq, bool pending) override;

    std::optional<int> DequeuePendingIRQ() override;

    void Reset();
};
