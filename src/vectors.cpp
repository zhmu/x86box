#include "vectors.h"
#include <assert.h>
#include "cpux86.h"
#include "memory.h"
#include "xcallablevector.h"

namespace
{
    //Interrupts base address in memory
    constexpr inline unsigned int VectorHandlerSegment = 0xf800;
}

Vectors::Vectors(Memory& oMemory) : m_Memory(oMemory) {}

void Vectors::Register(uint8_t no, XCallableVector& oCallback)
{
    const auto result = m_Map.insert({ no, oCallback });
    assert(result.second); // vector already registered if this fails

    /*
     * Now set up the vector itself; we use a pseudo-instruction to call it
     * because we intend to let existing code hook our vectors.
     */
    CPUx86::addr_t addr = CPUx86::MakeAddr(VectorHandlerSegment, no * 4);
    m_Memory.WriteWord(addr + 0, 0x340f); // x86CPU extension: invoke vector
    m_Memory.WriteByte(addr + 2, no);     // vector number
    m_Memory.WriteByte(addr + 3, 0xcf);   // IRETF

    // Hook the interrupt table
    CPUx86::addr_t vector_addr = CPUx86::MakeAddr(0, no * 4);
    m_Memory.WriteWord(vector_addr + 0, no * 4);
    m_Memory.WriteWord(vector_addr + 2, VectorHandlerSegment);
}

void Vectors::Invoke(CPUx86& oCPU, uint8_t no)
{
    const auto it = m_Map.find(no);
    assert(it != m_Map.end()); // vector not registered if this fails

    // First of all, invoke the handler
    auto& state = oCPU.GetState();
    it->second.InvokeVector(no, oCPU, state);

    /*
     * XXX We assume a PUSHF has been executed; overwrite the value in memory with
     * the new flags
     */
    oCPU.GetMemory().WriteWord(CPUx86::MakeAddr(state.m_ss, state.m_sp + 4), state.m_flags);
}
