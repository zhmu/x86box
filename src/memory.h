#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>
#include <list>

class XMemoryMapped;

class Memory
{
  public:
    typedef uint32_t addr_t;

    Memory();
    ~Memory();

    void Reset();

    uint8_t ReadByte(addr_t addr);
    uint16_t ReadWord(addr_t addr);

    void WriteByte(addr_t addr, uint8_t data);
    void WriteWord(addr_t addr, uint16_t data);

    void AddPeripheral(addr_t base, uint16_t length, XMemoryMapped& oPeripheral);

    void* GetPointer(addr_t addr, uint16_t length);
    char* GetASCIIZString(addr_t addr);

  protected:
    //! \brief Memory size
    static const unsigned int m_MemorySize = 1048576;

    //! \brief Maximum ASCIIZ string length
    static const unsigned int m_StringSize = 256;

    //! \brief Memory contents
    uint8_t* m_Memory;

    //! \brief Memory mapped peripheral
    class MemoryMapped
    {
      public:
        MemoryMapped(addr_t base, uint16_t length, XMemoryMapped& oPeripheral)
            : m_base(base), m_length(length), m_peripheral(oPeripheral)
        {
        }

        //! \brief Does the peripheral occupy this memory range?
        bool Matches(addr_t base) const;

        //! \brief Provides access to the peripheral
        XMemoryMapped* operator->() const;

      protected:
        addr_t m_base;
        uint16_t m_length;
        XMemoryMapped& m_peripheral;
    };

    typedef std::list<MemoryMapped> TPeripheralList;
    TPeripheralList m_peripheral;

    //! \brief Temporary ASCIIZ string
    char m_String[m_StringSize];
};

//! \brief Interface class for a memory-mapped device
class XMemoryMapped
{
  public:
    virtual ~XMemoryMapped() {}

    virtual uint8_t ReadByte(Memory::addr_t addr) = 0;
    virtual uint16_t ReadWord(Memory::addr_t addr) = 0;

    virtual void WriteByte(Memory::addr_t addr, uint8_t data) = 0;
    virtual void WriteWord(Memory::addr_t addr, uint16_t data) = 0;
};

inline bool Memory::MemoryMapped::Matches(addr_t addr) const
{
    return addr >= m_base && addr < m_base + m_length;
}

inline XMemoryMapped* Memory::MemoryMapped::operator->() const { return &m_peripheral; }

#endif /* __MEMORY_H__ */
