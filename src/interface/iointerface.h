#pragma once

using io_port = uint16_t;

struct IOPeripheral
{
    virtual void Out8(io_port port, uint8_t val) = 0;
    virtual void Out16(io_port port, uint16_t val) = 0;
    virtual uint8_t In8(io_port port) = 0;
    virtual uint16_t In16(io_port port) = 0;
};

struct IOInterface
{
    virtual void AddPeripheral(io_port base, uint16_t length, IOPeripheral& peripheral) = 0;
    virtual void Out8(io_port port, uint8_t val) = 0;
    virtual void Out16(io_port port, uint16_t val) = 0;

    virtual uint8_t In8(io_port port) = 0;
    virtual uint16_t In16(io_port port) = 0;
};
