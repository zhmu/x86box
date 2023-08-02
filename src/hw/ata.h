#pragma once

#include <memory>

struct IOInterface;

class ImageProvider;

class ATA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    ATA(IOInterface& io, ImageProvider&);
    ~ATA();

    void Reset();
};
