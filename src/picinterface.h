#pragma once

#include <optional>

struct PICInterface
{
    enum class IRQ {
      PIT,
      Keyboard,
      Cascade,
      COM2,
      COM1,
      LPT,
      FDC,
      LPT1
    };
    virtual void AssertIRQ(IRQ irq) = 0;
    virtual void SetPendingIRQState(IRQ irq, bool pending) = 0;

    virtual std::optional<int> DequeuePendingIRQ() = 0;
};
