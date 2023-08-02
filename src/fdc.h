#pragma once

#include <memory>

struct DMAInterface;
struct IOInterface;
struct PICInterface;
class PIC;
class ImageProvider;

class FDC final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    FDC(IOInterface&, PICInterface&, DMAInterface&, ImageProvider&);
    ~FDC();

    void Reset();
    void NotifyImageChanged();
};
