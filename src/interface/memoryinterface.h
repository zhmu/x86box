#pragma once

namespace memory
{
    using Address = uint32_t;
}

class MemoryMappedPeripheral
{
  public:
    virtual uint8_t ReadByte(memory::Address addr) = 0;
    virtual uint16_t ReadWord(memory::Address addr) = 0;

    virtual void WriteByte(memory::Address addr, uint8_t data) = 0;
    virtual void WriteWord(memory::Address addr, uint16_t data) = 0;
};

struct MemoryInterface
{
    virtual uint8_t ReadByte(memory::Address addr) = 0;
    virtual uint16_t ReadWord(memory::Address addr) = 0;

    virtual void WriteByte(memory::Address addr, uint8_t data) = 0;
    virtual void WriteWord(memory::Address addr, uint16_t data) = 0;

    virtual void AddPeripheral(memory::Address base, uint16_t length, MemoryMappedPeripheral& peripheral) = 0;

    virtual void* GetPointer(memory::Address addr, uint16_t length) = 0;
};
