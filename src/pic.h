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

    void AssertIRQ(int num);

    std::optional<int> DequeuePendingIRQ();

    void Reset();
};
