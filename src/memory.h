#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "memoryinterface.h"

class Memory final : public MemoryInterface
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Memory();
    ~Memory();

    void Reset();

    uint8_t ReadByte(memory::Address addr) override;
    uint16_t ReadWord(memory::Address addr) override;

    void WriteByte(memory::Address addr, uint8_t data) override;
    void WriteWord(memory::Address addr, uint16_t data) override;

    void AddPeripheral(memory::Address base, uint16_t length, MemoryMappedPeripheral& peripheral) override;

    void* GetPointer(memory::Address addr, uint16_t length);
    std::string GetASCIIZString(memory::Address addr);
};

