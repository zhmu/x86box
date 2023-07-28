#pragma once

#include "io.h"
#include <memory>

class ImageProvider;

class ATA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    ATA(IO& io, ImageProvider&);
    ~ATA();

    void Reset();
};