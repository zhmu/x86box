#ifndef __VGA_H__
#define __VGA_H__

#include <memory>
#include "memory.h"

class IO;
class HostIO;
class Vectors;

class VGA : public XMemoryMapped
{
  public:
    VGA(Memory& memory, IO& io, HostIO& hostio);
    virtual ~VGA();

    virtual void Reset();
    virtual void Update();

    virtual uint8_t ReadByte(Memory::Address addr);
    virtual uint16_t ReadWord(Memory::Address addr);

    virtual void WriteByte(Memory::Address addr, uint8_t data);
    virtual void WriteWord(Memory::Address addr, uint16_t data);

    // XXX Resolution for now
    static constexpr inline unsigned int s_video_width = 640;
    static constexpr inline unsigned int s_video_height = 400;

  protected:
    std::unique_ptr<uint8_t[]> m_videomem;

    HostIO& m_hostio;
    Memory& m_memory;
    IO& m_io;
};

#endif /* __VGA_H__ */
