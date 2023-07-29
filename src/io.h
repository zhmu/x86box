#ifndef __IO_H__
#define __IO_H__

#include <cstdint>
#include <memory>

using io_port = uint16_t;

struct IOPeripheral
{
    virtual void Out8(io_port port, uint8_t val) = 0;
    virtual void Out16(io_port port, uint16_t val) = 0;
    virtual uint8_t In8(io_port port) = 0;
    virtual uint16_t In16(io_port port) = 0;
};

class IO final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

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
