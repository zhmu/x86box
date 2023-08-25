#include "vga.h"
#include <cstdio>
#include <cstring>

#include "../interface/iointerface.h"
#include "../interface/memoryinterface.h"
#include "../interface/tickinterface.h"
#include "../platform/hostio.h"
#include "vgafont.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// http://www.osdever.net/FreeVGA/vga/vga.htm
namespace
{
    static const unsigned int VideoMemorySize = 262144;

    // Pixel clock = 25.175 MHz
    static const double PixelClock = 25'175'000.0;

    // http://tinyvga.com/vga-timing/640x400@70Hz
    static const unsigned int HSyncVisibleArea = 640;
    static const unsigned int HSyncFrontPorch = 16;
    static const unsigned int HSyncSyncPulse = 96;
    static const unsigned int HSyncBackPorch = 48;
    static const unsigned int WholeLineHSyncCounter =
        HSyncVisibleArea + HSyncFrontPorch + HSyncSyncPulse + HSyncBackPorch;

    static const unsigned int VSyncVisibleArea = 400;
    static const unsigned int VSyncFrontPorch = 12;
    static const unsigned int VSyncSyncPulse = 2;
    static const unsigned int VSyncBackPorch = 35;
    static const unsigned int WholeFrameVSyncCounter =
        VSyncVisibleArea + VSyncFrontPorch + VSyncSyncPulse + VSyncBackPorch;

    static const unsigned int WholeFrame = WholeLineHSyncCounter * WholeFrameVSyncCounter;

    uint64_t NsToPixels(std::chrono::nanoseconds ns)
    {
        const auto pixelsPerNs = PixelClock / 1'000'000'000.0;
        return static_cast<uint64_t>(ns.count() * pixelsPerNs);
    }

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
        namespace color {
            static constexpr inline io_port CRTCControllerAddress = 0x3d4;
            static constexpr inline io_port CRTCControllerData = 0x3d5;
            static constexpr inline io_port InputStatus1_Read = 0x3da;
            static constexpr inline io_port FeatureControl_Write = 0x3da;
        }
        namespace mono {
            static constexpr inline io_port CRTCControllerAddress = 0x3b4;
            static constexpr inline io_port CRTCControllerData = 0x3b5;
            static constexpr inline io_port InputStatus1_Read = 0x3ba;
            static constexpr inline io_port FeatureControl_Write = 0x3ba;
        }
    }

    // http://www.osdever.net/FreeVGA/vga/crtcreg.htm
    enum class crtc
    {
        HorizontalTotal = 0x00,
        EndHorizontalDisplay = 0x01,
        StartHorizontalBlanking = 0x02,
        EndHorizontalBlanking = 0x03,
        StartHorizontalRetrace = 0x04,
        EndHorizontalRetrace = 0x05,
        VerticalTotal = 0x06,
        Overflow = 0x07,
        PresentRowScan = 0x08,
        MaximumScanLine = 0x09,
        CursorStart = 0x0a,
        CursorEnd = 0x0b,
        StartAddressHigh = 0x0c,
        StartAddressLow = 0x0d,
        CursorLocationHigh = 0x0e,
        CursorLocationLow = 0x0f,
        VerticalRetraceStart = 0x10,
        VerticalRetraceEnd = 0x11,
        VerticalDisplayEnd = 0x12,
        Offset = 0x13,
        UnderlineLocation = 0x14,
        StartVerticalBlanking = 0x15,
        EndVerticalBlanking = 0x16,
        CRTCModeControl = 0x17,
        LineCompare = 0x18,
    };

    enum class attr
    {
        Palette0 = 0x00,
        Palette1 = 0x01,
        Palette2 = 0x02,
        Palette3 = 0x03,
        Palette4 = 0x04,
        Palette5 = 0x05,
        Palette6 = 0x06,
        Palette7 = 0x07,
        Palette8 = 0x08,
        Palette9 = 0x09,
        Palette10 = 0x0a,
        Palette11 = 0x0b,
        Palette12 = 0x0c,
        Palette13 = 0x0d,
        Palette14 = 0x0e,
        Palette15 = 0x0f,
        AttributeModeControl = 0x10,
        OverscanColor = 0x11,
        ColorPlanEnable = 0x12,
        HorizonalPelPlanning = 0x13,
        ColorSelect = 0x14,
    };

    namespace input_status_1
    {
        static constexpr inline uint8_t DisplayEnable = 0b0000'0001;
        static constexpr inline uint8_t VerticalRetrace = 0b0000'1000;
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

struct VGA::Impl : MemoryMappedPeripheral, IOPeripheral
{
    std::shared_ptr<spdlog::logger> logger;
    HostIO& hostio;
    TickInterface& tick;
    std::chrono::nanoseconds first_tick{};
    uint64_t current_frame_counter{};
    std::array<uint8_t, VideoMemorySize> videomem{};

    uint8_t crtc_address{};
    std::array<uint8_t, 25> crtc_reg{};

    bool attr_flipflop{};
    uint8_t attr_address{};
    std::array<uint8_t, 21> attr_reg{};

    uint16_t hsync_counter{};
    uint16_t vsync_counter{};

    Impl(MemoryInterface& memory, IOInterface& io, HostIO& hostio, TickInterface& tick);
    ~Impl();

    uint8_t ReadByte(memory::Address addr) override;
    uint16_t ReadWord(memory::Address addr) override;

    void WriteByte(memory::Address addr, uint8_t data) override;
    void WriteWord(memory::Address addr, uint16_t data) override;

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;

    bool Update();
};


VGA::Impl::Impl(MemoryInterface& memory, IOInterface& io, HostIO& hostio, TickInterface& tick)
    : hostio(hostio)
    , tick(tick)
    , logger(spdlog::stderr_color_st("vga"))
{
    memory.AddPeripheral(0xa0000, 65535, *this);
    memory.AddPeripheral(0xb0000, 65535, *this);
    io.AddPeripheral(0x3b0, 47, *this);
}

VGA::Impl::~Impl()
{
    spdlog::drop("vga");
}

bool VGA::Impl::Update()
{
    bool draw = false;

    const auto delta = tick.GetTickCount() - first_tick;
    const auto delta_in_pixels = NsToPixels(delta);
    const auto this_frame_number = delta_in_pixels / WholeFrame;
    const auto this_frame_delta = delta_in_pixels % WholeFrame;

    hsync_counter = this_frame_delta % WholeLineHSyncCounter;
    vsync_counter = this_frame_delta / WholeLineHSyncCounter;
    //std::cout << "hsync " << hsync_counter << " vsync " << vsync_counter << '\n';
    //logger->info("hsync {} vsync {}", hsync_counter, vsync_counter);

    // XXX Only render every few frames to keep things speedy
    if (current_frame_counter + 5 >= this_frame_number)
        return false;
    //logger->critical("rendering frame {}", this_frame_number);
    current_frame_counter = this_frame_number;

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

    return true;
}

VGA::VGA(MemoryInterface& memory, IOInterface& io, HostIO& hostio, TickInterface& tick)
    : impl(std::make_unique<Impl>(memory, io, hostio, tick))
{
    Reset();
}

VGA::~VGA() = default;

void VGA::Reset()
{
    std::fill(impl->videomem.begin(), impl->videomem.end(), 0);
    impl->first_tick = impl->tick.GetTickCount();
    impl->current_frame_counter = 0;
}

uint8_t VGA::Impl::ReadByte(memory::Address addr)
{
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        return videomem[addr - 0xb8000];
    }
    return 0;
}

uint16_t VGA::Impl::ReadWord(memory::Address addr)
{
    if (addr >= 0xb8000 && addr <= 0xb8fff - 1) {
        const auto a = videomem[addr - 0xb8000 + 0];
        const auto b = videomem[addr - 0xb8000 + 1];
        return a | (static_cast<uint16_t>(b) << 8);
    } else {
        return 0;
    }
}

void VGA::Impl::WriteByte(memory::Address addr, uint8_t data)
{
    if (addr >= 0xb8000 && addr <= 0xb8fff) {
        videomem[addr - 0xb8000] = data;
    }
}

void VGA::Impl::WriteWord(memory::Address addr, uint16_t data)
{
    if (addr >= 0xb8000 && addr <= 0xb8fff - 1) {
        videomem[addr - 0xb8000 + 0] = data & 0xff;
        videomem[addr - 0xb8000 + 1] = data >> 8;
    }
}

void VGA::Impl::Out8(io_port port, uint8_t val)
{
    logger->info("out8({:x}, {:x}", port, val);
    switch(port)
    {
        case io::AttributeAddressData:
            if (attr_flipflop) {
                attr_reg[attr_address % attr_reg.size()] = val;
            } else {
                attr_address = val;
            }
            attr_flipflop = !attr_flipflop;
            break;
        case io::color::CRTCControllerAddress:
        case io::mono::CRTCControllerAddress:
            crtc_address = val;
            break;
        case io::color::CRTCControllerData:
        case io::mono::CRTCControllerData:
            crtc_reg[crtc_address % crtc_reg.size()] = val;
            break;
    }
}

uint8_t VGA::Impl::In8(io_port port)
{
    if (port != io::color::InputStatus1_Read && port != io::mono::InputStatus1_Read)
        logger->info("in8({:x})", port);
    switch(port) {
        case io::color::InputStatus1_Read:
        case io::mono::InputStatus1_Read: {
            attr_flipflop = false;
            const bool hsync = (hsync_counter < HSyncVisibleArea + HSyncFrontPorch) ||
                               (hsync_counter >= WholeLineHSyncCounter - HSyncBackPorch);
            const bool vsync = (vsync_counter < VSyncVisibleArea + VSyncFrontPorch) ||
                              (vsync_counter >= WholeFrameVSyncCounter - VSyncBackPorch);

            uint8_t value = 0;
            if (hsync) value |= 1;
            if (vsync) value |= 8;
            return value;
        }
        case io::color::CRTCControllerAddress:
        case io::mono::CRTCControllerAddress:
            return crtc_address;
        case io::color::CRTCControllerData:
        case io::mono::CRTCControllerData:
            return crtc_reg[crtc_address % crtc_reg.size()];
    }
    return 0;
}

void VGA::Impl::Out16(io_port port, uint16_t val)
{
    logger->info("out16({:x}, {:x}", port, val);
    Out8(port, val & 0xff);
    Out8(port + 1, val >> 8);
}

uint16_t VGA::Impl::In16(io_port port)
{
    logger->info("in16({:x})", port);
    return 0;
}

bool VGA::Update()
{
    return impl->Update();
}
