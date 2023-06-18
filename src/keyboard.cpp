#include "keyboard.h"
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "hostio.h"

#include "spdlog/spdlog.h"

Keyboard::Keyboard(Memory& memory, IO& io, HostIO& hostio)
    : m_memory(memory), m_hostio(hostio), m_io(io)
{
}

Keyboard::~Keyboard() = default;

void Keyboard::Reset()
{
#if 0
    m_memory.WriteByte(CPUx86::MakeAddr(0x40, 0x49), 3);           // current video mode
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4a), 80);          // columns on screen
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4c), 80 * 25 * 2); // page size in bytes
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4e), 0);           // page start address
    m_memory.WriteByte(CPUx86::MakeAddr(0x40, 0x62), 0);           // page number
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x63), 0x3d4);       // crt base i/o port
#endif
}
