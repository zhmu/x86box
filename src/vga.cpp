#include "vga.h"
#include <stdio.h>
#include <string.h>

#include "hostio.h"
#include "vgafont.h"
#include "vectors.h"

#define TRACE_INT(x...) \
    if (1)              \
    fprintf(stderr, "[vga-int] " x)
#define TRACE(x...) \
    if (1)          \
    fprintf(stderr, "[vga] " x)

VGA::VGA(Memory& memory, HostIO& hostio, Vectors& vectors)
    : m_memory(memory), m_hostio(hostio), m_vectors(vectors)
{
    m_videomem = new uint8_t[m_memorysize];
}

VGA::~VGA() { delete[] m_videomem; }

void VGA::Reset()
{
    memset(m_videomem, 0, m_memorysize);
    m_vectors.Register(0x10, *this);

    m_memory.WriteByte(CPUx86::MakeAddr(0x40, 0x49), 3);           // current video mode
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4a), 80);          // columns on screen
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4c), 80 * 25 * 2); // page size in bytes
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x4e), 0);           // page start address
    m_memory.WriteByte(CPUx86::MakeAddr(0x40, 0x62), 0);           // page number
    m_memory.WriteWord(CPUx86::MakeAddr(0x40, 0x63), 0x3d4);       // crt base i/o port
}

uint8_t VGA::ReadByte(Memory::Address addr)
{
    TRACE("read(8) @ 0x%04x\n", addr);
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        return m_videomem[addr - 0xb8000];
    }
    return 0;
}

uint16_t VGA::ReadWord(Memory::Address addr)
{
    TRACE("read(16) @ 0x%04x\n", addr);
    return ReadByte(addr) | ReadByte(addr + 1) << 8;
}

void VGA::WriteByte(Memory::Address addr, uint8_t data)
{
    TRACE("write(8) @ 0x%04x data=0x%02x\n", addr, data);
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        m_videomem[addr - 0xb8000] = data;
    }
}

void VGA::WriteWord(Memory::Address addr, uint16_t data)
{
    TRACE("write(16) @ 0x%04x data=0x%04x\n", addr, data);
    WriteByte(addr, data & 0xff);
    WriteByte(addr + 1, data >> 8);
}

void VGA::Update()
{
    for (unsigned int y = 0; y < 25; y++)
        for (unsigned int x = 0; x < 80; x++) {
            uint8_t ch = m_videomem[160 * y + 2 * x + 0];
            uint8_t cl = m_videomem[160 * y + 2 * x + 1];
            uint8_t* d = &font_data[ch * 8];
            for (unsigned int j = 0; j < 8; j++)
                for (unsigned int i = 0; i < 8; i++) {
                    uint32_t color = 0x00ffffff;
                    if ((d[j] & (1 << (8 - i))) == 0)
                        color = 0;
                    m_hostio.putpixel(x * 8 + i, y * 8 + j, color);
                }
        }
}

void VGA::InvokeVector(uint8_t no, CPUx86& oCPU, CPUx86::State& oState)
{
#define GET_AL uint8_t al = oState.m_ax & 0xff;
#define GET_BH uint8_t bh = (oState.m_bx & 0xff00) >> 8;

    uint8_t ah = (oState.m_ax & 0xff00) >> 8;
    switch (ah) {
        case 0x0f:
            TRACE_INT("ah=%02x: get current video mode\n", ah);
            oState.m_ax = m_memory.ReadByte(CPUx86::MakeAddr(0x40, 0x49)) |
                          m_memory.ReadByte(CPUx86::MakeAddr(0x40, 0x4a)) << 8;
            oState.m_bx = (oState.m_bx & 0xff) | m_memory.ReadByte(CPUx86::MakeAddr(0x40, 0x62))
                                                     << 8;
            break;
        case 0x08: {
            GET_BH;
            TRACE_INT(
                "ah=%02x: read character and attribute at cursor position, page=%u\n", ah, bh);
            oState.m_ax = 0x1e20;
            break;
        }
        case 0x11: {
            GET_AL;
            switch (al) {
                case 0x30:
                    TRACE_INT("ax=%04x: get font information\n", oState.m_ax);
                    oState.m_es = 0;
                    oState.m_bp = 0;                           /* XXX */
                    oState.m_cx = 0;                           // XXX
                    oState.m_dx = (oState.m_dx & 0xff00) | 25; // XXX
                    break;
                default:
                    TRACE_INT("unknown function ax=%04x\n", oState.m_ax);
            }
            break;
        }
        default: /* what's this? */
            TRACE_INT("unknown function ah=%02x\n", ah);
            break;
    }
}
