#ifndef __VGA_H__
#define __VGA_H__

#include <memory>
#include "memory.h"
#include "io.h"

class IO;
class HostIO;
class Vectors;

class VGA : XMemoryMapped, IOPeripheral
{
  public:
    VGA(Memory& memory, IO& io, HostIO& hostio);
    virtual ~VGA();

    virtual void Reset();
    virtual void Update();

    uint8_t ReadByte(Memory::Address addr) override;
    uint16_t ReadWord(Memory::Address addr) override;

    void WriteByte(Memory::Address addr, uint8_t data) override;
    void WriteWord(Memory::Address addr, uint16_t data) override;

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;

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
