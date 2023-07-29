#pragma once

#include <memory>
#include <optional>

class DMA;
class IO;
class PIC;
class ImageProvider;

class FDC final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    FDC(IO&, PIC&, DMA&, ImageProvider&);
    ~FDC();

    void Reset();
    void NotifyImageChanged();
};