#pragma once

#include <cstdint>
#include "state.h"

struct IOInterface;
struct MemoryInterface;

class CPUx86
{
  public:
    using addr_t = uint32_t;

    CPUx86(MemoryInterface& oMemory, IOInterface& oIO);
    ~CPUx86();

    void RunInstruction();
    void Reset();

    cpu::State& GetState() { return m_State; }
    const cpu::State& GetState() const { return m_State; }

    static addr_t MakeAddr(uint16_t seg, uint16_t off);

    void HandleInterrupt(uint8_t no);

  private:
    MemoryInterface& m_Memory;
    IOInterface& m_IO;
    cpu::State m_State;
};
