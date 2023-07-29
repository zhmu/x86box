#ifndef __VGA_H__
#define __VGA_H__

#include <memory>

class HostIO;
class IO;
class Memory;

class VGA final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    VGA(Memory& memory, IO& io, HostIO& hostio);
    ~VGA();

    virtual void Reset();
    virtual void Update();

    // XXX Resolution for now
    static constexpr inline unsigned int s_video_width = 640;
    static constexpr inline unsigned int s_video_height = 400;
};

#endif /* __VGA_H__ */
