#include "vga.h"
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "hostio.h"
#include "memory.h"
#include "vgafont.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// http://www.osdever.net/FreeVGA/vga/vga.htm
namespace
{
    static const unsigned int VideoMemorySize = 262144;

    namespace io {
        static constexpr inline io_port AttributeAddressData = 0x3c0;
        static constexpr inline io_port AttributeData = 0x3c1;
        static constexpr inline io_port InputStatus0_Read = 0x3c2;
        static constexpr inline io_port MiscOutput_Write = 0x3c2;
        static constexpr inline io_port SequencerAddress = 0x3c4;
        static constexpr inline io_port SequencerData = 0x3c5;
        static constexpr inline io_port DACState_Read = 0x3c7;
        static constexpr inline io_port DACAddressReadMode_Write = 0x3c7;
        static constexpr inline io_port DACAddressWriteMode = 0x3c8;
        static constexpr inline io_port DACData = 0x3c9;
        static constexpr inline io_port FeatureControl_Read = 0x3ca;
        static constexpr inline io_port MiscOutput_Read  = 0x3cc;
        static constexpr inline io_port GraphicsControllerAddress = 0x3ce;
        static constexpr inline io_port GraphicsControllerData = 0x3cf;
        static constexpr inline io_port CRTCControllerAddress = 0x3d4;
        static constexpr inline io_port CRTCControllerData = 0x3d5;
        static constexpr inline io_port InputStatus1_Read = 0x3da;
        static constexpr inline io_port FeatureControl_Write = 0x3da;
    }

    constexpr uint32_t SwapRGBtoBGR(uint32_t v)
    {
        return (v & 0x00ff00) | ((v & 0xff) << 16) | ((v & 0xff0000) >> 16);
    }
    static_assert(SwapRGBtoBGR(0x123456) == 0x563412);

    // https://moddingwiki.shikadi.net/wiki/B800_Text
    constexpr std::array<uint32_t, 16> egaPalette{
        SwapRGBtoBGR(0x000000), SwapRGBtoBGR(0x0000aa), SwapRGBtoBGR(0x00aa00), SwapRGBtoBGR(0x00aaaa),
        SwapRGBtoBGR(0xaa0000), SwapRGBtoBGR(0xaa00aa), SwapRGBtoBGR(0xaa5500), SwapRGBtoBGR(0xaaaaaa),
        SwapRGBtoBGR(0x555555), SwapRGBtoBGR(0x5555ff), SwapRGBtoBGR(0x55ff55), SwapRGBtoBGR(0x55ffff),
        SwapRGBtoBGR(0xff5555), SwapRGBtoBGR(0xff55ff), SwapRGBtoBGR(0xffff55), SwapRGBtoBGR(0xffffff)
    };
}

struct VGA::Impl : XMemoryMapped, IOPeripheral
{
    std::shared_ptr<spdlog::logger> logger;
    HostIO& hostio;
    std::array<uint8_t, VideoMemorySize> videomem{};

    Impl(Memory& memory, IO& io, HostIO& hostio);

    uint8_t ReadByte(Memory::Address addr) override;
    uint16_t ReadWord(Memory::Address addr) override;

    void WriteByte(Memory::Address addr, uint8_t data) override;
    void WriteWord(Memory::Address addr, uint16_t data) override;

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;


    void Update();
};


VGA::Impl::Impl(Memory& memory, IO& io, HostIO& hostio)
    : hostio(hostio)
    , logger(spdlog::stderr_color_st("vga"))
{
    memory.AddPeripheral(0xa0000, 65535, *this);
    memory.AddPeripheral(0xb0000, 65535, *this);
    io.AddPeripheral(0x3c0, 31, *this);
}

void VGA::Impl::Update()
{
    for (unsigned int y = 0; y < 25; y++)
        for (unsigned int x = 0; x < 80; x++) {
            const auto ch = videomem[160 * y + 2 * x + 0];
            const auto cl = videomem[160 * y + 2 * x + 1];
            const auto d = &font_data[ch * 8];
            for (unsigned int j = 0; j < 8; j++)
                for (unsigned int i = 0; i < 8; i++) {
                    uint32_t color{};
                    if ((d[j] & (1 << (8 - i))) == 0)
                        color = egaPalette[cl >> 4];
                    else
                        color = egaPalette[cl & 0xf];
                    hostio.putpixel(x * 8 + i, y * 8 + j, color);
                }
        }
}

VGA::VGA(Memory& memory, IO& io, HostIO& hostio)
    : impl(std::make_unique<Impl>(memory, io, hostio))
{
}

VGA::~VGA() = default;

void VGA::Reset()
{
    std::fill(impl->videomem.begin(), impl->videomem.end(), 0);
}

uint8_t VGA::Impl::ReadByte(Memory::Address addr)
{
    logger->info("read(8) @ 0x{:4x}", addr);
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        return videomem[addr - 0xb8000];
    }
    return 0;
}

uint16_t VGA::Impl::ReadWord(Memory::Address addr)
{
    logger->info("read(16) @ 0x{:4x}", addr);
    if (addr >= 0xb8000 && addr <= 0xb8fff - 1) {
        const auto a = videomem[addr - 0xb8000 + 0];
        const auto b = videomem[addr - 0xb8000 + 1];
        return a | (static_cast<uint16_t>(b) << 8);
    } else {
        return 0;
    }
}

void VGA::Impl::WriteByte(Memory::Address addr, uint8_t data)
{
    logger->info("write(8) @ 0x{:4x} data=0x{:04x}", addr, data);
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        videomem[addr - 0xb8000] = data;
    }
}

void VGA::Impl::WriteWord(Memory::Address addr, uint16_t data)
{
    logger->info("write(16) @ 0x{:4x} data=0x{:04x}", addr, data);
    if (addr >= 0xb8000 && addr <= 0xb8fff - 1) {
        videomem[addr - 0xb8000 + 0] = data & 0xff;
        videomem[addr - 0xb8000 + 1] = data >> 8;
    }
}

void VGA::Impl::Out8(io_port port, uint8_t val)
{
    logger->info("out8({:x}, {:x}", port, val);
}

uint8_t VGA::Impl::In8(io_port port)
{
    logger->info("in8({:x})", port);
    switch(port) {
        case io::InputStatus1_Read: {
            // XXX Toggle HSync Output (bit 0) and Vertical Refresh (bit 3)
            static uint8_t value = 0;
            value = value ^ 9;
            return value;
        }
    }
    return 0;
}

void VGA::Impl::Out16(io_port port, uint16_t val)
{
    logger->info("out16({:x}, {:x}", port, val);
}

uint16_t VGA::Impl::In16(io_port port)
{
    logger->info("in16({:x})", port);
    return 0;
}

void VGA::Update()
{
    impl->Update();
}