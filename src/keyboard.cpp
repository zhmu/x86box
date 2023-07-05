#include "keyboard.h"
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "hostio.h"

#include "spdlog/spdlog.h"

namespace
{
    namespace io
    {
        constexpr inline io_port Base = 0x60;

        constexpr inline io_port Data = Base + 0x0;
        constexpr inline io_port Status_Read = Base + 0x4;
        constexpr inline io_port Command_Write = Base + 0x4;
    }
}

struct Keyboard::Impl : IOPeripheral
{
    HostIO& hostio;
    uint16_t scancode = 0;

    Impl(IO&, HostIO&);

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};

Keyboard::Impl::Impl(IO& io, HostIO& hostio)
    : hostio(hostio)
{
    io.AddPeripheral(io::Data, 1, *this);
    io.AddPeripheral(io::Status_Read, 1, *this);
}

Keyboard::Keyboard(IO& io, HostIO& hostio)
    : impl(std::make_unique<Impl>(io, hostio))
{
}

Keyboard::~Keyboard() = default;

void Keyboard::Reset()
{
    impl->scancode = 0;
}

void Keyboard::EnqueueScancode(uint16_t scancode)
{
    impl->scancode = scancode;
}

void Keyboard::Impl::Out8(io_port port, uint8_t val)
{
    spdlog::info("keyboard: out8({:x}, {:x})", port, val);
}

void Keyboard::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("keyboard: out16({:x}, {:x})", port, val);
}

uint8_t Keyboard::Impl::In8(io_port port)
{
    spdlog::info("keyboard: in8({:x})", port);
    switch(port) {
        case io::Data: {
            if (const auto value = scancode >> 8; value != 0) {
                scancode = scancode >> 8;
                return value;
            }
            const auto v = scancode;
            scancode = 0;
            return v;
        }
    }
    return 0;
}

uint16_t Keyboard::Impl::In16(io_port port)
{
    spdlog::info("keyboard: in16({:x})", port);
    return 0;
}
