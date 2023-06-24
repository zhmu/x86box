#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include <memory>

class HostIO;
class IO;

class Keyboard final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    Keyboard(IO& io, HostIO& hostio);
    ~Keyboard();

    virtual void Reset();
};

#endif /* __KEYBOARD_H__ */
