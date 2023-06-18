#include "io.h"
#include <stdio.h>

#include "spdlog/spdlog.h"

IO::IO() {}

IO::~IO() {}

void IO::Reset() {}

void IO::Out8(io_port port, uint8_t val)
{
    if (auto p = FindPeripheral(port); p) {
        p->Out8(port, val);
    } else {
        spdlog::warn("Out8(): ignoring write to unmapped port {:x} (value {:x})", port, val);
    }
}

void IO::Out16(io_port port, uint16_t val)
{
    if (auto p = FindPeripheral(port); p) {
        p->Out16(port, val);
    } else {
        spdlog::warn("Out16(): ignoring write to unmapped port {:x} (value {:x})", port, val);
    }
}

uint8_t IO::In8(io_port port)
{
    if (auto p = FindPeripheral(port); p) {
        return p->In8(port);
    } else {
        spdlog::warn("In8(): read from unmapped port {:x}", port);
        return 0;
    }
}

uint16_t IO::In16(io_port port)
{
    if (auto p = FindPeripheral(port); p) {
        return p->In16(port);
    } else {
        spdlog::warn("In16(): read from unmapped port {:x}", port);
        return 0;
    }
}

void IO::AddPeripheral(io_port base, uint16_t length, IOPeripheral& peripheral)
{
    peripherals.emplace_back(base, length, peripheral);
}

IOPeripheral* IO::FindPeripheral(const io_port addr)
{
    auto it = std::find_if(peripherals.begin(), peripherals.end(), [&](const auto& p) {
        return p.Matches(addr);
    });
    return it != peripherals.end() ? &it->peripheral : nullptr;
}
