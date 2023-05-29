#include "keyboard.h"
#include <stdio.h>
#include <string.h>

#include "hostio.h"
#include "vectors.h"

#define TRACE_INT(x...) \
    if (1)              \
    fprintf(stderr, "[kbd-int] " x)
#define TRACE(x...) \
    if (1)          \
    fprintf(stderr, "[kbd] " x)

Keyboard::Keyboard(Memory& memory, HostIO& hostio, Vectors& vectors)
    : m_memory(memory), m_hostio(hostio), m_vectors(vectors)
{
}

Keyboard::~Keyboard() = default;

void Keyboard::Reset()
{
    m_vectors.Register(0x16, *this);
#if 0
    m_memory.WriteByte(CPUx86::MakeAddr(0x40, 0x49), 3);           // current video mode
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4a), 80);          // columns on screen
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4c), 80 * 25 * 2); // page size in bytes
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4e), 0);           // page start address
    m_memory.WriteByte(CPUx86::MakeAddr(0x40, 0x62), 0);           // page number
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x63), 0x3d4);       // crt base i/o port
#endif
}

void Keyboard::InvokeVector(uint8_t no, CPUx86& oCPU, cpu::State& oState)
{
    const uint8_t ah = (oState.m_ax & 0xff00) >> 8;
    switch (ah) {
        case 0x01: {
            TRACE_INT("ah=%02x: check for key\n", ah);
            oState.m_flags |= cpu::flag::ZF;
            break;
        }
        default: /* what's this? */
            TRACE_INT("unknown function ah=%02x\n", ah);
            break;
    }
}
