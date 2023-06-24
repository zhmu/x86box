#pragma once

#include "io.h"

class ATA : IOPeripheral
{
  public:
    ATA(IO& io);
    virtual ~ATA() = default;

    void Reset();

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};