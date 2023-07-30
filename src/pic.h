#pragma once

#include <memory>
#include <optional>

class IO;

class PIC final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    PIC(IO& io);
    ~PIC();

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

    void AssertIRQ(IRQ irq);
    void SetPendingIRQState(IRQ irq, bool pending);

    std::optional<int> DequeuePendingIRQ();

    void Reset();
};
