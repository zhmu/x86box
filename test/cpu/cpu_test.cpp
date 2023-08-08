#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "interface/iointerface.h"
#include "interface/memoryinterface.h"
#include "cpu/cpux86.h"
#include "bus/memory.h"
#include <array>

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

    struct CPUTest : ::testing::Test
    {
        IOMock io;
        testing::NiceMock<MemoryMock> memory;
        CPUx86 cpu;

        CPUTest() : cpu(memory, io)
        {
            cpu.Reset();
            auto& state = cpu.GetState();
            state.m_cs = initialIp >> 4;
            state.m_ip = initialIp & 0xf;
            state.m_ax = 0; state.m_bx = 0; state.m_cx = 0; state.m_dx = 0;
            state.m_si = 0; state.m_di = 0; state.m_bp = 0;

            state.m_ss = initialStack >> 4;
            state.m_sp = initialStack & 0xf;

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
        }

        template<size_t N>
        void RunCodeBytes(const std::array<uint8_t, N>& bytes)
        {
            Sequence s;
            for(size_t n = 0; n < bytes.size(); ++n) {
                EXPECT_CALL(memory, ReadByte(initialIp + n))
                    .Times(1)
                    .InSequence(s)
                    .WillOnce(Return(bytes[n]));
            }

            while(true) {
                const auto& state = cpu.GetState();
                if (CPUx86::MakeAddr(state.m_cs, state.m_ip) == initialIp + bytes.size())
                    break;
                cpu.RunInstruction();
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
