#ifndef __IO_H__
#define __IO_H__

#include <cstdint>
#include <vector>

using io_port = uint16_t;

struct IOPeripheral
{
    virtual void Out8(io_port port, uint8_t val) = 0;
    virtual void Out16(io_port port, uint16_t val) = 0;
    virtual uint8_t In8(io_port port) = 0;
    virtual uint16_t In16(io_port port) = 0;
};

class IO
{
    struct Peripheral
    {
        const io_port base;
        const uint16_t length;
        IOPeripheral& peripheral;

        bool Matches(io_port port) const {
            return port >= base && port < base + length;
        }
    };
    IOPeripheral* FindPeripheral(const io_port addr);

    std::vector<Peripheral> peripherals;

public:
    IO();
    ~IO();

    void Reset();
    void AddPeripheral(io_port base, uint16_t length, IOPeripheral& peripheral);

    void Out8(io_port port, uint8_t val);
    void Out16(io_port port, uint16_t val);

    uint8_t In8(io_port port);
    uint16_t In16(io_port port);
};

#endif /* __IO_H__ */
