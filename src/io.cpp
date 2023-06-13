#include "io.h"
#include <stdio.h>

#include "spdlog/spdlog.h"

IO::IO() {}

IO::~IO() {}

void IO::Reset() {}

void IO::Out8(port_t port, uint8_t val) { spdlog::info("out8(): port={:x} val=0x{:x}", port, val); }

void IO::Out16(port_t port, uint16_t val) { spdlog::info("out16(): port={:x} val=0x{:x}", port, val); }

uint8_t IO::In8(port_t port)
{
    spdlog::info("in8(): port={:x}", port);
    static uint8_t b = 0;
    b ^= 1;
    return b;
}

uint16_t IO::In16(port_t port)
{
    spdlog::info("in16(): port={:x}", port);
    return 0;
}
