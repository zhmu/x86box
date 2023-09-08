#pragma once

#include "interface/iointerface.h"
#include "interface/memoryinterface.h"
#include "cpu/cpux86.h"
#include "cpu/state.h"
#include "bus/memory.h"
#include <span>

namespace cpu_helper
{
    using ::testing::Return;

    constexpr inline uint32_t initialIp = 0x4'000;
    constexpr inline uint32_t initialStack = 0x2'000;

    struct IOMock : IOInterface
    {
        MOCK_METHOD(void, AddPeripheral, (io_port base, uint16_t length, IOPeripheral& peripheral), (override));
        MOCK_METHOD(void, Out8, (io_port port, uint8_t val), (override));
        MOCK_METHOD(void, Out16, (io_port port, uint16_t val), (override));
        MOCK_METHOD(uint8_t, In8, (io_port port), (override));
        MOCK_METHOD(uint16_t, In16, (io_port port), (override));
    };

    class MemoryMock : public MemoryInterface
    {
        Memory memory;
    public:
        MemoryMock()
        {
            // By default, pass-through memory access as usual
            ON_CALL(*this, ReadWord).WillByDefault([&](memory::Address addr) {
                return memory.ReadWord(addr);
            });
            ON_CALL(*this, ReadByte).WillByDefault([&](memory::Address addr) {
                return memory.ReadByte(addr);
            });
            ON_CALL(*this, WriteWord).WillByDefault([&](memory::Address addr, uint16_t value) {
                memory.WriteWord(addr, value);
            });
            ON_CALL(*this, WriteByte).WillByDefault([&](memory::Address addr, uint8_t value) {
                memory.WriteByte(addr, value);
            });
        }

        MOCK_METHOD(uint8_t, ReadByte, (memory::Address addr), (override));
        MOCK_METHOD(uint16_t, ReadWord, (memory::Address addr), (override));

        MOCK_METHOD(void, WriteByte, (memory::Address addr, uint8_t data), (override));
        MOCK_METHOD(void, WriteWord, (memory::Address addr, uint16_t data), (override));

        MOCK_METHOD(void, AddPeripheral, (memory::Address base, uint16_t length, MemoryMappedPeripheral& peripheral), (override));

        MOCK_METHOD(void*, GetPointer, (memory::Address addr, uint16_t length), (override));

        void StoreBytes(memory::Address base, std::span<const uint8_t> bytes)
        {
            for(size_t n = 0; n < bytes.size(); ++n)
                memory.WriteByte(base + n, bytes[n]);
        }
    };

    class TestHelper
    {
        CPUx86& cpu;
        MemoryMock& memory;

        auto& State() { return cpu.GetState(); }

    public:
        TestHelper(CPUx86& cpu, MemoryMock& memory)
            : cpu(cpu), memory(memory)
        {
            Reset();
        }

        template<uint16_t Flag>
        auto& Set()
        {
            cpu::SetFlag<Flag>(State().m_flags, true);
            return *this;
        }
        auto& CF() { return Set<cpu::flag::CF>(); }
        auto& ZF() { return Set<cpu::flag::ZF>(); }
        auto& SF() { return Set<cpu::flag::SF>(); }
        auto& OF() { return Set<cpu::flag::OF>(); }
        auto& PF() { return Set<cpu::flag::PF>(); }
        auto& DF() { return Set<cpu::flag::DF>(); }

        auto& AX(const uint16_t value) { State().m_ax = value; return *this; }
        auto& CX(const uint16_t value) { State().m_cx = value; return *this; }
        auto& DI(const uint16_t value) { State().m_di = value; return *this; }
        auto& SI(const uint16_t value) { State().m_si = value; return *this; }

        auto& ES(const uint16_t value) { State().m_es = value; return *this; }
        auto& DS(const uint16_t value) { State().m_ds = value; return *this; }

        auto& VerifyAX(const uint16_t value) { EXPECT_EQ(State().m_ax, value); return *this; }
        auto& VerifySI(const uint16_t value) { EXPECT_EQ(State().m_si, value); return *this; }
        auto& VerifyDI(const uint16_t value) { EXPECT_EQ(State().m_di, value); return *this; }
        auto& VerifyZF(const bool zf) { EXPECT_EQ(cpu::FlagZero(State().m_flags), zf); return *this; }

        auto& ExpectReadByte(memory::Address addr) {
            EXPECT_CALL(memory, ReadByte(addr));
            return *this;
        }

        auto& ExpectReadByte(memory::Address addr, const uint8_t value) {
            EXPECT_CALL(memory, ReadByte(addr)).WillOnce(Return(value));
            return *this;
        }

        auto& ExpectReadWord(memory::Address addr, const uint16_t value) {
            EXPECT_CALL(memory, ReadWord(addr)).WillOnce(Return(value));
            return *this;
        }

        auto& ExpectWriteByte(memory::Address addr, const uint8_t value) {
            EXPECT_CALL(memory, WriteByte(addr, value));
            return *this;
        }

        auto& ExpectWriteWord(memory::Address addr, const uint16_t value) {
            EXPECT_CALL(memory, WriteWord(addr, value));
            return *this;
        }

        void Reset()
        {
            cpu.Reset();
            auto& state = cpu.GetState();
            state.m_cs = initialIp >> 4;
            state.m_ip = initialIp & 0xf;
            state.m_ax = 0; state.m_bx = 0; state.m_cx = 0; state.m_dx = 0;
            state.m_si = 0; state.m_di = 0; state.m_bp = 0;

            state.m_ss = initialStack >> 4;
            state.m_sp = initialStack & 0xf;
        }

        auto& Execute(std::span<const uint8_t> bytes)
        {
            memory.StoreBytes(initialIp, bytes);
            while(true) {
                const auto& state = cpu.GetState();
                if (CPUx86::MakeAddr(state.m_cs, state.m_ip) == initialIp + bytes.size())
                    break;
                cpu.RunInstruction();
            }
            return *this;
        }
    };

    struct Test : ::testing::Test
    {
        IOMock io;
        testing::NiceMock<MemoryMock> memory;
        CPUx86 cpu;
        TestHelper th{ cpu, memory };

        Test() : cpu(memory, io)
        {
        }

        using StateBuildFn = void(*)(TestHelper&);
        using TestHelperValue = std::pair<StateBuildFn, StateBuildFn>;

        void RunTests(StateBuildFn initial_state, std::span<const uint8_t> code_bytes, std::span<const TestHelperValue> tests)
        {
            for(const auto& [ set_state, verify_state ] : tests) {
                TestHelper th{ cpu, memory };
                initial_state(th);
                set_state(th);
                th.Execute(code_bytes);
                verify_state(th);
            }
        }
    };
}