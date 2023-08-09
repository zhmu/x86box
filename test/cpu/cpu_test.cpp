#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "interface/iointerface.h"
#include "interface/memoryinterface.h"
#include "cpu/cpux86.h"
#include "cpu/state.h"
#include "bus/memory.h"
#include <array>
#include <span>

using ::testing::Return;
using ::testing::Sequence;

namespace
{
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

    struct MemoryMock : Memory
    {
        MOCK_METHOD(uint8_t, ReadByte, (memory::Address addr), (override));
        MOCK_METHOD(uint16_t, ReadWord, (memory::Address addr), (override));

        MOCK_METHOD(void, WriteByte, (memory::Address addr, uint8_t data), (override));
        MOCK_METHOD(void, WriteWord, (memory::Address addr, uint16_t data), (override));

        MOCK_METHOD(void, AddPeripheral, (memory::Address base, uint16_t length, MemoryMappedPeripheral& peripheral), (override));

        MOCK_METHOD(void*, GetPointer, (memory::Address addr, uint16_t length), (override));
    };

    struct StateBuilder
    {
        cpu::State& state;

        template<uint16_t Flag>
        auto& Set()
        {
            cpu::SetFlag<Flag>(state.m_flags, true);
            return *this;
        }

        auto& CF() { return Set<cpu::flag::CF>(); }
        auto& ZF() { return Set<cpu::flag::ZF>(); }
        auto& SF() { return Set<cpu::flag::SF>(); }
        auto& OF() { return Set<cpu::flag::OF>(); }
        auto& PF() { return Set<cpu::flag::PF>(); }

        auto& AX(const uint16_t value) { state.m_ax = value; return *this; }
        auto& CX(const uint16_t value) { state.m_cx = value; return *this; }
    };

    struct CPUTest : ::testing::Test
    {
        IOMock io;
        testing::NiceMock<MemoryMock> memory;
        CPUx86 cpu;

        CPUTest() : cpu(memory, io)
        {
            // By default, pass-through memory access as usual
            ON_CALL(memory, ReadWord).WillByDefault([&](memory::Address addr) {
                return memory.Memory::ReadWord(addr);
            });
            ON_CALL(memory, ReadByte).WillByDefault([&](memory::Address addr) {
                return memory.Memory::ReadByte(addr);
            });
            ON_CALL(memory, WriteWord).WillByDefault([&](memory::Address addr, uint16_t value) {
                memory.Memory::WriteWord(addr, value);
            });
            ON_CALL(memory, WriteByte).WillByDefault([&](memory::Address addr, uint8_t value) {
                memory.Memory::WriteByte(addr, value);
            });

            Reset(cpu);
        }

        void Reset(CPUx86& cpu)
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

        void RunCodeBytes(std::span<const uint8_t> bytes)
        {
            for(size_t n = 0; n < bytes.size(); ++n) {
                ON_CALL(memory, ReadByte(initialIp + n))
                    .WillByDefault(Return(bytes[n]));
            }

            while(true) {
                const auto& state = cpu.GetState();
                if (CPUx86::MakeAddr(state.m_cs, state.m_ip) == initialIp + bytes.size())
                    break;
                cpu.RunInstruction();
            }
        }

        using StateBuildFn = void(*)(StateBuilder&);
        using StateBuilderValue = std::pair<StateBuildFn, uint16_t>;

        void RunStateBuilderAXTest(StateBuildFn initial_state, std::span<const uint8_t> code_bytes, std::span<const StateBuilderValue> tests)
        {
            for(const auto& [ set_state, expected_ax ] : tests) {
                Reset(cpu);
                StateBuilder sb{ cpu.GetState() };
                initial_state(sb);
                set_state(sb);
                RunCodeBytes(code_bytes);
                EXPECT_EQ(cpu.GetState().m_ax, expected_ax);
            }
        }
    };
}

TEST_F(CPUTest, Instantiation)
{
}

TEST_F(CPUTest, FlagsHighNibbleBitsAreSet)
{
    RunCodeBytes(std::to_array<uint8_t>({ 0x9c, 0x58 })); /* pushf, pop ax */
    EXPECT_EQ(0xf002, cpu.GetState().m_ax);
}

TEST_F(CPUTest, FlagsHighNibbleBitsCannotBeCleared)
{
    RunCodeBytes(std::to_array<uint8_t>({
        0x31, 0xdb, /* xor bx,bx */
        0x53,       /* push bx */
        0x9d,       /* popf */
        0x9c,       /* pushf */
        0x58        /* pop ax */
    }));
    EXPECT_EQ(0xf002, cpu.GetState().m_ax);
}

TEST_F(CPUTest, JA_JNBE)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x77, 0x03,         // ja +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.ZF(); }, 0 },
        { [](auto& sb) { sb.CF(); }, 0 },
        { [](auto& sb) { sb.ZF().CF(); }, 0 },
    }));
}

TEST_F(CPUTest, JAE_JNB_JNC)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x73, 0x03,         // jnc +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.CF(); }, 0 },
    }));
}

TEST_F(CPUTest, JB_JC_JNAE)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x72, 0x03,         // jc +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.CF(); }, 1 },
    }));
}

TEST_F(CPUTest, JBE_JNA)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x76, 0x03,         // jna +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.CF(); }, 1 },
        { [](auto& sb) { sb.ZF(); }, 1 },
        { [](auto& sb) { sb.ZF().CF(); }, 1 },
    }));
}

TEST_F(CPUTest, JCXZ)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0xe3, 0x03,         // jcxz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { sb.CX(0); }, 1 },
        { [](auto& sb) { sb.CX(1); }, 0 },
    }));
}

TEST_F(CPUTest, JE_JZ)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x74, 0x03,         // jz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.ZF(); }, 1 },
    }));
}

TEST_F(CPUTest, JG_JNLE)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x7f, 0x03,         // jg +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.ZF(); }, 0 },
        { [](auto& sb) { sb.SF(); }, 0 },
        { [](auto& sb) { sb.SF().OF(); }, 1 },
        { [](auto& sb) { sb.OF(); }, 0 },
        { [](auto& sb) { sb.ZF().OF(); }, 0 },
    }));
}

TEST_F(CPUTest, JGE_JNL)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x7d, 0x03,         // jge +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.SF(); }, 0 },
        { [](auto& sb) { sb.OF(); }, 0 },
        { [](auto& sb) { sb.SF().OF(); }, 1 },
    }));
}

TEST_F(CPUTest, JL_JNGE)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x7c, 0x03,         // jl +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.SF(); }, 1 },
        { [](auto& sb) { sb.OF(); }, 1 },
        { [](auto& sb) { sb.SF().OF(); }, 0 },
    }));
}

TEST_F(CPUTest, JLE_JNG)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x7e, 0x03,         // jl +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.ZF(); }, 1 },
        { [](auto& sb) { sb.SF(); }, 1 },
        { [](auto& sb) { sb.OF(); }, 1 },
        { [](auto& sb) { sb.SF().OF(); }, 0 },
        { [](auto& sb) { sb.ZF().SF().OF(); }, 1 },
    }));
}

TEST_F(CPUTest, JNE_JNZ)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x75, 0x03,         // jnz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.ZF(); }, 0 },
    }));
}

TEST_F(CPUTest, JNO)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x71, 0x03,         // jno +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.OF(); }, 0 },
    }));
}

TEST_F(CPUTest, JNP_JPO)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x7b, 0x03,         // jnp +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.PF(); }, 0 },
    }));
}

TEST_F(CPUTest, JNS)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x79, 0x03,         // jns +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 1 },
        { [](auto& sb) { sb.SF(); }, 0 },
    }));
}

TEST_F(CPUTest, JO)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x70, 0x03,         // jo +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.OF(); }, 1 },
    }));
}

TEST_F(CPUTest, JP_JPE)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x7a, 0x03,         // jp +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.PF(); }, 1 },
    }));
}

TEST_F(CPUTest, JS)
{
    RunStateBuilderAXTest([](auto& sb) {
        sb.AX(1);
    }, std::to_array<uint8_t>({
        0x78, 0x03,         // js +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }), std::to_array<StateBuilderValue>({
        { [](auto& sb) { }, 0 },
        { [](auto& sb) { sb.SF(); }, 1 },
    }));
}