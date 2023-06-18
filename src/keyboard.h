#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "memory.h"

class IO;
class HostIO;
class Vectors;

class Keyboard
{
  public:
    Keyboard(Memory& memory, IO& io, HostIO& hostio);
    virtual ~Keyboard();

    virtual void Reset();

  protected:
    HostIO& m_hostio;
    Memory& m_memory;
    IO& m_io;
};

#endif /* __KEYBOARD_H__ */
