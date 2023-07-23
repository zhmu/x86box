#pragma once

#include "io.h"
#include <memory>

class ATA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    ATA(IO& io);
    ~ATA();

    void Reset();
};