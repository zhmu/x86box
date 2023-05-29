#include "memory.h"
#include <string.h>
#include <algorithm>

#include <stdio.h>

namespace
{
    static const unsigned int memorySize = 1048576;
    static const unsigned int m_StringSize = 256;
}

Memory::Memory() { m_Memory = std::make_unique<uint8_t[]>(memorySize); }
Memory::~Memory() = default;

void Memory::Reset() { memset(m_Memory.get(), 0, memorySize); }

uint8_t Memory::ReadByte(Address addr)
{
    const auto p = FindPeripheralByAddress(addr);
    if (p) return p->ReadByte(addr);
    return m_Memory[addr];
}

uint16_t Memory::ReadWord(Address addr)
{
    const auto p = FindPeripheralByAddress(addr);
    if (p) return p->ReadWord(addr);
    return m_Memory[addr] | (uint16_t)m_Memory[addr + 1] << 8;
}

void Memory::WriteByte(Address addr, uint8_t data)
{
    const auto p = FindPeripheralByAddress(addr);
    if (p) {
        p->WriteByte(addr, data);
    } else {
        m_Memory[addr] = data;
    }
}

void Memory::WriteWord(Address addr, uint16_t data)
{
    const auto p = FindPeripheralByAddress(addr);
    if (p) {
        p->WriteWord(addr, data);
    } else {
        m_Memory[addr + 0] = data & 0xff;
        m_Memory[addr + 1] = data >> 8;
    }
}

void Memory::AddPeripheral(Address base, uint16_t length, XMemoryMapped& oPeripheral)
{
    m_peripheral.push_back(MemoryMapped(base, length, oPeripheral));
}

std::string Memory::GetASCIIZString(Address addr)
{
    std::string s;
    while(true) {
        const auto v = ReadByte(addr++);
        if (v == 0)
            break;
        s += v;
    }
    return s;
}

void* Memory::GetPointer(Address addr, uint16_t length)
{
    auto p = FindPeripheralByAddress(addr);
    if (p) return nullptr;
    return &m_Memory[addr];
}

XMemoryMapped* Memory::FindPeripheralByAddress(const Address addr)
{
    auto it = std::find_if(m_peripheral.begin(), m_peripheral.end(), [&](const auto& p) {
        return p.Matches(addr);
    });
    return it != m_peripheral.end() ? &it->m_peripheral : nullptr;
}
