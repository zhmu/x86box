#include "memory.h"
#include <string.h>
#include <algorithm>

#include <stdio.h>

namespace
{
    static constexpr size_t memorySize = 1048576;

    struct Mapping
    {
        const memory::Address base;
        const uint16_t length;
        MemoryMappedPeripheral& peripheral;

        bool Matches(memory::Address addr) const {
            return addr >= base && addr < base + length;
        }
    };
}

struct Memory::Impl
{
    std::unique_ptr<uint8_t[]> memory;

    std::vector<Mapping> mappings;

    Impl();
    void Reset();
    MemoryMappedPeripheral* FindPeripheralByAddress(const memory::Address addr);
};

Memory::Memory()
    : impl(std::make_unique<Impl>())
{
}

Memory::~Memory() = default;

Memory::Impl::Impl()
    : memory(std::make_unique<uint8_t[]>(memorySize))
{
    Reset();
}

void Memory::Impl::Reset()
{
    std::fill(memory.get(), memory.get() + memorySize, 0);
}

MemoryMappedPeripheral* Memory::Impl::FindPeripheralByAddress(const memory::Address addr)
{
    const auto it = std::find_if(mappings.begin(), mappings.end(), [&](const auto& p) {
        return p.Matches(addr);
    });
    return it != mappings.end() ? &it->peripheral : nullptr;
}

void Memory::Reset() { impl->Reset(); }

uint8_t Memory::ReadByte(memory::Address addr)
{
    if (const auto p = impl->FindPeripheralByAddress(addr); p)
        return p->ReadByte(addr);
    else
        return impl->memory[addr];
}

uint16_t Memory::ReadWord(memory::Address addr)
{
    if (const auto p = impl->FindPeripheralByAddress(addr); p)
        return p->ReadWord(addr);
    else
        return impl->memory[addr] | static_cast<uint16_t>(impl->memory[addr + 1]) << 8;
}

void Memory::WriteByte(memory::Address addr, uint8_t data)
{
    if (const auto p = impl->FindPeripheralByAddress(addr); p) {
        p->WriteByte(addr, data);
    } else {
        impl->memory[addr] = data;
    }
}

void Memory::WriteWord(memory::Address addr, uint16_t data)
{
    if (const auto p = impl->FindPeripheralByAddress(addr); p) {
        p->WriteWord(addr, data);
    } else {
        impl->memory[addr + 0] = data & 0xff;
        impl->memory[addr + 1] = data >> 8;
    }
}

void Memory::AddPeripheral(memory::Address base, uint16_t length, MemoryMappedPeripheral& peripheral)
{
    impl->mappings.push_back(Mapping(base, length, peripheral));
}

std::string Memory::GetASCIIZString(memory::Address addr)
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

void* Memory::GetPointer(memory::Address addr, uint16_t length)
{
    if (const auto p = impl->FindPeripheralByAddress(addr); p)
        return nullptr;
    else
        return &impl->memory[addr];
}
