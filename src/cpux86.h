#ifndef __CPUX86_H__
#define __CPUX86_H__

#include <stdint.h>
#include "state.h"

class IO;
class Memory;

using x86addr_t = uint32_t;

class DecodeState
{
  public:
    enum Type { T_REG, T_MEM };

    Type m_type;
    uint16_t m_seg, m_off;
    uint16_t m_reg;
    x86addr_t m_disp;
};

class CPUx86
{
  public:
    using addr_t = x86addr_t;

    CPUx86(Memory& oMemory, IO& oIO);
    ~CPUx86();

    /*! \brief Fetches and executes the next instruction
     *  \returns Number of clock cycles used
     */
    void RunInstruction();

    //! \brief Resets the CPU
    void Reset();

    void Dump();

    cpu::State& GetState() { return m_State; }
    const cpu::State& GetState() const { return m_State; }
    Memory& GetMemory() { return m_Memory; }
    const Memory& GetMemory() const { return m_Memory; }
    IO& GetIO() { return m_IO; }
    const IO& GetIO() const { return m_IO; }

    static addr_t MakeAddr(uint16_t seg, uint16_t off);

    void HandleInterrupt(uint8_t no);

  protected:
    void Push16(uint16_t value);
    uint16_t Pop16();

    DecodeState m_DecodeState;
    void DecodeEA(uint8_t modrm, DecodeState& oState);

    void SignalInterrupt(uint8_t no);

    // Interrupt values
    static const unsigned int INT_DIV_BY_ZERO = 0;
    static const unsigned int INT_SINGLE_STEP = 1;
    static const unsigned int INT_NMI = 2;
    static const unsigned int INT_BREAKPOINT = 3;
    static const unsigned int INT_OVERFLOW = 4;

  private:
    uint8_t GetNextOpcode();

    Memory& m_Memory;
    IO& m_IO;
    cpu::State m_State;
};

#endif /* __CPUX86_H__ */
