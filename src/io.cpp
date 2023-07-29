#include "io.h"

#include <vector>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace {
    struct Peripheral
    {
        const io_port base;
        const uint16_t length;
        IOPeripheral& peripheral;

        bool Matches(io_port port) const {
            return port >= base && port < base + length;
        }
    };
}

struct IO::Impl
{
    std::vector<Peripheral> peripherals;
    std::shared_ptr<spdlog::logger> logger;

    Impl();
    IOPeripheral* FindPeripheral(const io_port addr);
};

IO::Impl::Impl()
    : logger(spdlog::stderr_color_st("io"))
{
}

IOPeripheral* IO::Impl::FindPeripheral(const io_port addr)
{
    auto it = std::find_if(peripherals.begin(), peripherals.end(), [&](const auto& p) {
        return p.Matches(addr);
    });
    return it != peripherals.end() ? &it->peripheral : nullptr;
}

IO::IO()
    : impl(std::make_unique<Impl>())
{
}

IO::~IO() = default;

void IO::Reset() {}

void IO::Out8(io_port port, uint8_t val)
{
    if (auto p = impl->FindPeripheral(port); p) {
        p->Out8(port, val);
    } else {
        impl->logger->warn("out8(): ignoring write to unmapped port {:x} (value {:x})", port, val);
    }
}

void IO::Out16(io_port port, uint16_t val)
{
    if (auto p = impl->FindPeripheral(port); p) {
        p->Out16(port, val);
    } else {
        impl->logger->warn("out16(): ignoring write to unmapped port {:x} (value {:x})", port, val);
    }
}

uint8_t IO::In8(io_port port)
{
    if (auto p = impl->FindPeripheral(port); p) {
        return p->In8(port);
    } else {
        impl->logger->warn("in8(): read from unmapped port {:x}", port);
        return 0;
    }
}

uint16_t IO::In16(io_port port)
{
    if (auto p = impl->FindPeripheral(port); p) {
        return p->In16(port);
    } else {
        impl->logger->warn("in16(): read from unmapped port {:x}", port);
        return 0;
    }
}

void IO::AddPeripheral(io_port base, uint16_t length, IOPeripheral& peripheral)
{
    impl->peripherals.emplace_back(base, length, peripheral);
}
