#ifndef __IO_H__
#define __IO_H__

#include <cstdint>
#include <memory>
#include "iointerface.h"

class IO final : public IOInterface
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    IO();
    ~IO();
    void Reset();
    void AddPeripheral(io_port base, uint16_t length, IOPeripheral& peripheral) override;

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;

    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};

#endif /* __IO_H__ */
