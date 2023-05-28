#include "io.h"
#include <stdio.h>

#define TRACE(x...) fprintf(stderr, "[io] " x)

IO::IO() {}

IO::~IO() {}

void IO::Reset() {}

void IO::Out8(port_t port, uint8_t val) { TRACE("out8(): port=0x%x val=0x%x\n", port, val); }

void IO::Out16(port_t port, uint16_t val) { TRACE("out16(): port=0x%x val=0x%x\n", port, val); }

uint8_t IO::In8(port_t port)
{
    TRACE("in8(): port=0x%x\n", port);
    static uint8_t b = 0;
    b ^= 1;
    return b;
}

uint16_t IO::In16(port_t port)
{
    TRACE("in16(): port=0x%x\n", port);
    return 0;
}
