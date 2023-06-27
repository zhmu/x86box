#pragma once

#include <memory>
#include <optional>

class DMA;
class IO;
class PIC;

class FDC final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    FDC(IO&, PIC&, DMA&);
    ~FDC();

    void Reset();
};