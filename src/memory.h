#pragma once

#include <cstdint>
#include <vector>
#include <memory>

class XMemoryMapped;

class Memory
{
  public:
    using Address = uint32_t;

    Memory();
    ~Memory();

    void Reset();

    uint8_t ReadByte(Address addr);
    uint16_t ReadWord(Address addr);

    void WriteByte(Address addr, uint8_t data);
    void WriteWord(Address addr, uint16_t data);

    void AddPeripheral(Address base, uint16_t length, XMemoryMapped& oPeripheral);

    void* GetPointer(Address addr, uint16_t length);
    std::string GetASCIIZString(Address addr);

  private:
    std::unique_ptr<uint8_t[]> m_Memory;

    //! \brief Memory mapped peripheral
    struct MemoryMapped
    {
        const Address m_base;
        const uint16_t m_length;
        XMemoryMapped& m_peripheral;

        bool Matches(Address addr) const {
            return addr >= m_base && addr < m_base + m_length;
        }
    };
    std::vector<MemoryMapped> m_peripheral;

    XMemoryMapped* FindPeripheralByAddress(const Address addr);
};

//! \brief Interface class for a memory-mapped device
class XMemoryMapped
{
  public:
    virtual ~XMemoryMapped() {}

    virtual uint8_t ReadByte(Memory::Address addr) = 0;
    virtual uint16_t ReadWord(Memory::Address addr) = 0;

    virtual void WriteByte(Memory::Address addr, uint8_t data) = 0;
    virtual void WriteWord(Memory::Address addr, uint16_t data) = 0;
};
