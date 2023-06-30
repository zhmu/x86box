#ifndef __CPUX86_H__
#define __CPUX86_H__

#include <stdint.h>
#include <variant>
#include "state.h"

class IO;
class Memory;

struct DecodeState;

class CPUx86
{
  public:
    using addr_t = uint32_t;

    CPUx86(Memory& oMemory, IO& oIO);
    ~CPUx86();

    /*! \brief Fetches and executes the next instruction
     *  \returns Number of clock cycles used
     */
    void RunInstruction();

    //! \brief Resets the CPU
    void Reset();

    void Dump();

    const cpu::State& GetState() const { return m_State; }

    static addr_t MakeAddr(uint16_t seg, uint16_t off);

    void HandleInterrupt(uint8_t no);

  private:

    DecodeState DecodeEA(uint8_t modrm);

    void SignalInterrupt(uint8_t no);

    Memory& m_Memory;
    IO& m_IO;
    cpu::State m_State;
};

#endif /* __CPUX86_H__ */
