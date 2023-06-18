#include "vga.h"
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "hostio.h"
#include "vgafont.h"

#include "spdlog/spdlog.h"

namespace
{
    static const unsigned int VideoMemorySize = 262144;
}

VGA::VGA(Memory& memory, IO& io, HostIO& hostio)
    : m_memory(memory), m_hostio(hostio), m_io(io)
{
    m_videomem = std::make_unique<uint8_t[]>(VideoMemorySize);

    memory.AddPeripheral(0xa0000, 65535, *this);
    memory.AddPeripheral(0xb0000, 65535, *this);
}

VGA::~VGA() = default;

void VGA::Reset()
{
    memset(m_videomem.get(), 0, VideoMemorySize);
}

uint8_t VGA::ReadByte(Memory::Address addr)
{
    spdlog::info("vga: read(8) @ 0x{:4x}", addr);
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        return m_videomem[addr - 0xb8000];
    }
    return 0;
}

uint16_t VGA::ReadWord(Memory::Address addr)
{
    spdlog::info("vga: read(16) @ 0x{:4x}", addr);
    return ReadByte(addr) | ReadByte(addr + 1) << 8;
}

void VGA::WriteByte(Memory::Address addr, uint8_t data)
{
    spdlog::info("vga: write(8) @ 0x{:4x} data=0x{:04x}", addr, data);
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        m_videomem[addr - 0xb8000] = data;
    }
}

void VGA::WriteWord(Memory::Address addr, uint16_t data)
{
    spdlog::info("vga: write(16) @ 0x{:4x} data=0x{:04x}", addr, data);
    WriteByte(addr, data & 0xff);
    WriteByte(addr + 1, data >> 8);
}

void VGA::Update()
{
    for (unsigned int y = 0; y < 25; y++)
        for (unsigned int x = 0; x < 80; x++) {
            const auto ch = m_videomem[160 * y + 2 * x + 0];
            [[maybe_unused]] const auto cl = m_videomem[160 * y + 2 * x + 1];
            const auto d = &font_data[ch * 8];
            for (unsigned int j = 0; j < 8; j++)
                for (unsigned int i = 0; i < 8; i++) {
                    uint32_t color = 0x00ffffff;
                    if ((d[j] & (1 << (8 - i))) == 0)
                        color = 0;
                    m_hostio.putpixel(x * 8 + i, y * 8 + j, color);
                }
        }
}
