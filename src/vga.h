#ifndef __VGA_H__
#define __VGA_H__

#include <memory>

class HostIO;
struct IOInterface;
struct MemoryInterface;

class VGA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    VGA(MemoryInterface& memory, IOInterface& io, HostIO& hostio);
    ~VGA();

    void Reset();
    void Update();

    // XXX Resolution for now
    static constexpr inline unsigned int s_video_width = 640;
    static constexpr inline unsigned int s_video_height = 400;
};

#endif /* __VGA_H__ */
