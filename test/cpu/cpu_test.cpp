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

    struct CPUTest : ::testing::Test
    {
        IOMock io;
        testing::NiceMock<MemoryMock> memory;
        CPUx86 cpu;
        TestHelper th{ cpu, memory };

        CPUTest() : cpu(memory, io)
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

    void VerifyAXIsOne(TestHelper& th) { th.VerifyAX(1); }
    void VerifyAXIsZero(TestHelper& th) { th.VerifyAX(0); }
}

TEST_F(CPUTest, Instantiation)
{
}

TEST_F(CPUTest, FlagsHighNibbleBitsAreSet)
{
    th
        .Execute({{
            0x9c, 0x58 /* pushf, pop ax */
        }})
        .VerifyAX(0xf002);
}

TEST_F(CPUTest, FlagsHighNibbleBitsCannotBeCleared)
{
    th
        .Execute({{
            0x31, 0xdb, /* xor bx,bx */
            0x53,       /* push bx */
            0x9d,       /* popf */
            0x9c,       /* pushf */
            0x58        /* pop ax */
        }})
        .VerifyAX(0xf002);
}

TEST_F(CPUTest, JA_JNBE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x77, 0x03,         // ja +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsZero },
        { [](auto& th) { th.CF(); }, VerifyAXIsZero },
        { [](auto& th) { th.ZF().CF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JAE_JNB_JNC)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x73, 0x03,         // jnc +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.CF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JB_JC_JNAE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x72, 0x03,         // jc +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.CF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, JBE_JNA)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x76, 0x03,         // jna +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.CF(); }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsOne },
        { [](auto& th) { th.ZF().CF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, JCXZ)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0xe3, 0x03,         // jcxz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { th.CX(0); }, VerifyAXIsOne },
        { [](auto& th) { th.CX(1); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JE_JZ)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x74, 0x03,         // jz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.ZF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, JG_JNLE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7f, 0x03,         // jg +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsZero },
        { [](auto& th) { th.SF(); }, VerifyAXIsZero },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsZero },
        { [](auto& th) { th.ZF().OF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JGE_JNL)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7d, 0x03,         // jge +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.SF(); }, VerifyAXIsZero },
        { [](auto& th) { th.OF(); }, VerifyAXIsZero },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, JL_JNGE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7c, 0x03,         // jl +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.SF(); }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsOne },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JLE_JNG)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7e, 0x03,         // jl +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.ZF(); }, VerifyAXIsOne },
        { [](auto& th) { th.SF(); }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsOne },
        { [](auto& th) { th.SF().OF(); }, VerifyAXIsZero },
        { [](auto& th) { th.ZF().SF().OF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, JNE_JNZ)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x75, 0x03,         // jnz +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.ZF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JNO)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x71, 0x03,         // jno +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.OF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JNP_JPO)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7b, 0x03,         // jnp +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.PF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JNS)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x79, 0x03,         // jns +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsOne },
        { [](auto& th) { th.SF(); }, VerifyAXIsZero },
    }});
}

TEST_F(CPUTest, JO)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x70, 0x03,         // jo +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.OF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, JP_JPE)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x7a, 0x03,         // jp +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.PF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, JS)
{
    RunTests([](auto& th) {
        th.AX(1);
    }, {{
        0x78, 0x03,         // js +3
        0xb8, 0x00, 0x00    // mov ax,0x0
    }}, {{
        { [](auto& th) { }, VerifyAXIsZero },
        { [](auto& th) { th.SF(); }, VerifyAXIsOne },
    }});
}

TEST_F(CPUTest, STOSB)
{
    th
        .ExpectWriteByte(0x12345, 0x67)
        .ES(0x1000)
        .DI(0x2345)
        .AX(0x67)
        .Execute({{
            0xaa                // stosb
        }});
}

TEST_F(CPUTest, STOSW)
{
    th
        .ExpectWriteWord(0xabcde, 0x1378)
        .ES(0xa000)
        .DI(0xbcde)
        .AX(0x1378)
        .Execute({{
            0xab                // stosw
        }});
}

TEST_F(CPUTest, LODSB)
{
    th
        .ExpectReadByte(initialIp)
        .ExpectReadByte(0x12345, 0x1)
        .DS(0x1234)
        .SI(0x5)
        .AX(0xffff)
        .Execute({{
            0xac                // lodsb
        }})
        .VerifyAX(0xff01)
        .VerifySI(0x0006);
}

TEST_F(CPUTest, LODSW)
{
    th
        .ExpectReadWord(0x1000f, 0x9f03)
        .DS(0x1000)
        .SI(0xf)
        .AX(0xffff)
        .Execute({{
            0xad                // lodsw
        }})
        .VerifyAX(0x9f03)
        .VerifySI(0x0011);
}

TEST_F(CPUTest, MOVSB)
{
    th
        .ExpectReadByte(initialIp)
        .ExpectReadByte(0x1234a, 0x55)
        .ExpectWriteByte(0xf0027, 0x55)
        .DS(0x1234)
        .SI(0xa)
        .ES(0xf000)
        .DI(0x27)
        .Execute({{
            0xa4                // movsb
        }})
        .VerifySI(0x000b)
        .VerifyDI(0x0028);
}

TEST_F(CPUTest, MOVSW)
{
    th
        .ExpectReadByte(initialIp)
        .ExpectReadWord(0x23459, 0x55aa)
        .ExpectWriteWord(0x00003, 0x55aa)
        .DS(0x2345)
        .SI(0x9)
        .ES(0x0)
        .DI(0x3)
        .Execute({{
            0xa5                // movsw
        }})
        .VerifySI(0x000b)
        .VerifyDI(0x0005);
}

TEST_F(CPUTest, CMPSB_Matches)
{
    th
        .ExpectReadByte(initialIp)
        .ExpectReadByte(0x12345, 0x1)
        .ExpectReadByte(0x23456, 0x1)
        .DS(0x1234)
        .SI(0x5)
        .ES(0x2345)
        .DI(0x6)
        .Execute({{
            0xa6                // cmpsb
        }})
        .VerifySI(0x0006)
        .VerifyDI(0x0007)
        .VerifyZF(true);
}

TEST_F(CPUTest, CMPSB_Mismatches)
{
    th
        .ExpectReadByte(initialIp)
        .ExpectReadByte(0x12345, 0x1)
        .ExpectReadByte(0x23456, 0xfe)
        .DS(0x1234)
        .SI(0x5)
        .ES(0x2345)
        .DI(0x6)
        .Execute({{
            0xa6                // cmpsb
        }})
        .VerifySI(0x0006)
        .VerifyDI(0x0007)
        .VerifyZF(false);
}

TEST_F(CPUTest, SCASB_Matches)
{
    th
        .ExpectReadByte(initialIp)
        .ExpectReadByte(0x3434f, 0x94)
        .ES(0x3430)
        .DI(0x4f)
        .AX(0x94)
        .Execute({{
            0xae                // scasb
        }})
        .VerifyDI(0x0050)
        .VerifyZF(true);
}

TEST_F(CPUTest, SCASB_Mismatches)
{
    th
        .ExpectReadByte(initialIp)
        .ExpectReadByte(0x23900, 0x80)
        .ES(0x2390)
        .DI(0x0)
        .AX(0x94)
        .Execute({{
            0xae                // scasb
        }})
        .VerifyDI(0x0001)
        .VerifyZF(false);
}