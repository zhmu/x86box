#include "ppi.h"
#include "io.h"
#include "pit.h"

#include "spdlog/spdlog.h"

// Intel 8255
namespace
{
    namespace io
    {
        constexpr inline io_port Control = 0x61;
        constexpr inline io_port Switch = 0x62;
        constexpr inline io_port NMIMask = 0xa0;
    }

    namespace vid01 {
        constexpr inline uint8_t None = 0b00;
        constexpr inline uint8_t Color40x25 = 0b01;
        constexpr inline uint8_t Color80x25 = 0b10;
        constexpr inline uint8_t Monochrome = 0b11;
    }

    namespace switch_reg
    {
        constexpr inline uint8_t Timer2Output_1 = (1 << 4);
        constexpr inline uint8_t Timer2Output_2 = (1 << 5);
        constexpr inline uint8_t IOChannelCheck = (1 << 6);
        constexpr inline uint8_t RAMParityCheck = (1 << 7);
    }

    namespace sw_bits
    {
        constexpr inline uint8_t Fpu8087Installed = (1 << 1);
        constexpr inline uint8_t ZeroFloppyDrives = (0 << 6);
        constexpr inline uint8_t OneFloppyDrive = (1 << 6);
    }
}

struct PPI::Impl : IOPeripheral
{
    PIT& pit;

    uint8_t controlReg{};
    uint8_t selectedSwitchReg{};
    uint8_t switchReg{0b0000'0000};
    uint8_t sw{0b0000'0000};

    Impl(PIT& pit) : pit(pit) { }

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};

PPI::PPI(IO& io, PIT& pit)
    : impl(std::make_unique<Impl>(pit))
{
    io.AddPeripheral(io::Control, 1, *impl);
    io.AddPeripheral(io::Switch, 1, *impl);
    io.AddPeripheral(io::NMIMask, 1, *impl);
}

PPI::~PPI() = default;

void PPI::Reset()
{
}

void PPI::Impl::Out8(io_port port, uint8_t val)
{
    spdlog::info("ppi: out8({:x}, {:x})", port, val);
    switch(port) {
        case io::Control:
            selectedSwitchReg = (val & 2) != 0;
            break;
    }
}

void PPI::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("ppi: out16({:x}, {:x})", port, val);
}

uint8_t PPI::Impl::In8(io_port port)
{
    spdlog::info("ppi: in8({:x})", port);
    switch(port) {
        case io::Control: {
            uint8_t value = controlReg & 0xfe;
            if (pit.GetTimer2Output()) {
                value |= 1;
            }
            return value;
        }
        case io::Switch: {
            // Hi nibble is regardless of switch select
            uint8_t hiNibble = 0;
            if (pit.GetTimer2Output()) {
                hiNibble |= switch_reg::Timer2Output_1;
                hiNibble |= switch_reg::Timer2Output_2;
            }

            uint8_t loNibble;
            if (selectedSwitchReg == 0) /* Switch Select = 0 */ {
                // Bit 0,1 = VID0/VID1 (SW5-6 values ignored)
                // Bit 2,3 = Number of floppies (SW7-8 values)
                loNibble = (sw >> 4) & 0b1100;
                loNibble |= vid01::Color80x25;
            } else /*  Switch Select = 1 */ {
                // Bit 0 = 0 (SW1 value)
                // Bit 1 = 8087 installed (SW2 value)
                // Bit 2, 3 = On Board System Memory Size (SW3-4 values)
                loNibble = sw & 0xf;
            }
            return loNibble | hiNibble;
        }
    }
    return 0;
}

uint16_t PPI::Impl::In16(io_port port)
{
    spdlog::info("ppi: in16({:x})", port);
    return 0;
}
