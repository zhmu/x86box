#include "memory.h"
#include <string.h>

#include <stdio.h>

Memory::Memory() { m_Memory = new uint8_t[m_MemorySize]; }

Memory::~Memory() { delete[] m_Memory; }

void Memory::Reset() { memset(m_Memory, 0, m_MemorySize); }

#define MEMORY_PERIPHERAL_READ_OP(op)                                                           \
    for (TPeripheralList::iterator it = m_peripheral.begin(); it != m_peripheral.end(); it++) { \
        if (it->Matches(addr))                                                                  \
            return (*it)->op(addr);                                                             \
    }

#define MEMORY_PERIPHERAL_WRITE_OP(op)                                                          \
    for (TPeripheralList::iterator it = m_peripheral.begin(); it != m_peripheral.end(); it++) { \
        if (it->Matches(addr)) {                                                                \
            (*it)->op(addr, data);                                                              \
            return;                                                                             \
        }                                                                                       \
    }

uint8_t Memory::ReadByte(addr_t addr)
{
    MEMORY_PERIPHERAL_READ_OP(ReadByte);
    return m_Memory[addr];
}

uint16_t Memory::ReadWord(addr_t addr)
{
    MEMORY_PERIPHERAL_READ_OP(ReadWord);
    return m_Memory[addr] | (uint16_t)m_Memory[addr + 1] << 8;
}

void Memory::WriteByte(addr_t addr, uint8_t data)
{
    MEMORY_PERIPHERAL_WRITE_OP(WriteByte);
    m_Memory[addr] = data;
}

void Memory::WriteWord(addr_t addr, uint16_t data)
{
    MEMORY_PERIPHERAL_WRITE_OP(WriteWord);
    m_Memory[addr + 0] = data & 0xff;
    m_Memory[addr + 1] = data >> 8;
}

void Memory::AddPeripheral(addr_t base, uint16_t length, XMemoryMapped& oPeripheral)
{
    m_peripheral.push_back(MemoryMapped(base, length, oPeripheral));
}

char* Memory::GetASCIIZString(addr_t addr)
{
    char* ptr = m_String;
    for (unsigned int n = 0; n < m_StringSize; n++) {
        *ptr = ReadByte(addr++);
        if (*ptr == 0)
            return m_String;
        ptr++;
    }
    return NULL;
}

void* Memory::GetPointer(addr_t addr, uint16_t length)
{
    for (TPeripheralList::iterator it = m_peripheral.begin(); it != m_peripheral.end(); it++)
        if (it->Matches(addr))
            return NULL;

    return &m_Memory[addr];
}
