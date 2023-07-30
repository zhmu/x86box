#ifndef __CPUX86_H__
#define __CPUX86_H__

#include <stdint.h>
#include <variant>
#include "state.h"

class IO;
class Memory;

class CPUx86
{
  public:
    using addr_t = uint32_t;

    CPUx86(Memory& oMemory, IO& oIO);
    ~CPUx86();

    void RunInstruction();
    void Reset();

    const cpu::State& GetState() const { return m_State; }

    static addr_t MakeAddr(uint16_t seg, uint16_t off);

    void HandleInterrupt(uint8_t no);

  private:
    Memory& m_Memory;
    IO& m_IO;
    cpu::State m_State;
};

#endif /* __CPUX86_H__ */
