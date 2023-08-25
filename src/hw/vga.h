#pragma once

#include <memory>

class HostIO;
struct IOInterface;
struct MemoryInterface;
struct TickInterface;

class VGA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    VGA(MemoryInterface& memory, IOInterface& io, HostIO& hostio, TickInterface& tick);
    ~VGA();

    void Reset();
    bool Update();

    // XXX Resolution for now
    static constexpr inline unsigned int s_video_width = 640;
    static constexpr inline unsigned int s_video_height = 400;
};
