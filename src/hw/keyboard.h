#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include <memory>

class HostIO;
struct IOInterface;

class Keyboard final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    Keyboard(IOInterface& io, HostIO& hostio);
    ~Keyboard();

    virtual void Reset();
    bool IsQueueFilled() const;
    void EnqueueScancode(uint16_t scancode);
};

#endif /* __KEYBOARD_H__ */
