#ifndef __VGA_H__
#define __VGA_H__

#include "memory.h"
#include "xcallablevector.h"

class HostIO;
class Vectors;

class VGA : public XMemoryMapped, public XCallableVector
{
  public:
    VGA(Memory& memory, HostIO& hostio, Vectors& vectors);
    virtual ~VGA();

    virtual void Reset();
    virtual void Update();

    virtual uint8_t ReadByte(Memory::Address addr);
    virtual uint16_t ReadWord(Memory::Address addr);

    virtual void WriteByte(Memory::Address addr, uint8_t data);
    virtual void WriteWord(Memory::Address addr, uint16_t data);

    // XXX Resolution for now
    static const unsigned int s_video_width = 640;
    static const unsigned int s_video_height = 400;

    void InvokeVector(uint8_t no, CPUx86& oCPU, CPUx86::State& oState);

  protected:
    //! \brief Video memory
    uint8_t* m_videomem;

    //! \brief Video memory size
    static const unsigned int m_memorysize = 262144;

    //! \brief Host IO used
    HostIO& m_hostio;

    //! \brief Vectors object
    Vectors& m_vectors;

    //! \brief Host memory
    Memory& m_memory;
};

#endif /* __VGA_H__ */
