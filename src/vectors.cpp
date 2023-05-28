#include "vectors.h"
#include <assert.h>
#include "cpux86.h"
#include "memory.h"
#include "xcallablevector.h"

#include <stdio.h>

Vectors::Vectors(Memory& oMemory)
	: m_Memory(oMemory)
{
}

void
Vectors::Register(uint8_t no, XCallableVector& oCallback)
{
	std::pair<TintVectorHandlerMap::iterator, bool> res = m_Map.insert(std::pair<unsigned int, XCallableVector&>(no, oCallback));
	assert(res.second); // vector already registered if this fails

	/*
	 * Now set up the vector itself; we use a pseudo-instruction to call it
	 * because we intend to let existing code hook our vectors.
	 */
	CPUx86::addr_t addr = CPUx86::MakeAddr(m_VectorHandlerSegment, no * 4);
	m_Memory.WriteWord(addr + 0, 0x340f); // x86CPU extension: invoke vector
	m_Memory.WriteByte(addr + 2, no);     // vector number
	m_Memory.WriteByte(addr + 3, 0xcf);   // IRETF

	// Hook the interrupt table
	CPUx86::addr_t vector_addr = CPUx86::MakeAddr(0, no * 4);
	m_Memory.WriteWord(vector_addr + 0, no * 4);
	m_Memory.WriteWord(vector_addr + 2, m_VectorHandlerSegment);
}

void
Vectors::Invoke(CPUx86& oCPU, uint8_t no)
{
	TintVectorHandlerMap::iterator it = m_Map.find(no);
	assert(it != m_Map.end()); // vector not registered if this fails

	// First of all, invoke the handler
	CPUx86::State& oState = oCPU.GetState();
	it->second.InvokeVector(no, oCPU, oState);
	
	/*
	 * XXX We assume a PUSHF has been executed; overwrite the value in memory with
	 * the new flags
	 */
	oCPU.GetMemory().WriteWord(CPUx86::MakeAddr(oState.m_ss, oState.m_sp + 4), oState.m_flags);
}

/* vim:set ts=2 sw=2: */
