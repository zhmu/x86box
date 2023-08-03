#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "bus/memory.h"
#include <random>

using ::testing::Return;

namespace
{
    constexpr inline size_t memorySize = 1048576;
    constexpr inline memory::Address testPeriphalBase = 0x4'000;
    constexpr inline size_t testPeriphalSize = 1024;

    struct MockPeripheral : MemoryMappedPeripheral
    {
        MOCK_METHOD(uint8_t, ReadByte, (memory::Address addr), (override));
        MOCK_METHOD(uint16_t, ReadWord, (memory::Address addr), (override));

        MOCK_METHOD(void, WriteByte, (memory::Address addr, uint8_t data), (override));
        MOCK_METHOD(void, WriteWord, (memory::Address addr, uint16_t data), (override));
    };

    struct MemoryTest : ::testing::Test
    {
        Memory memory;
    };
}

TEST_F(MemoryTest, Instantiation)
{
}

TEST_F(MemoryTest, InitiallyAllMemoryIsZero)
{
    for(size_t n = 0; n < memorySize; ++n) {
        EXPECT_EQ(memory.ReadByte(n), 0) << "address " << n;
    }
}

TEST_F(MemoryTest, AllMemoryCanBeWrittenAsBytes)
{
    std::mt19937 rng(0);
    std::uniform_int_distribution dist(0, 255);

    for(size_t n = 0; n < memorySize; ++n) {
        const auto value = dist(rng);
        memory.WriteByte(n, value);
        EXPECT_EQ(value, memory.ReadByte(n)) << "address " << n;
    }
}

TEST_F(MemoryTest, MemoryCanBeAccessedUsingWords)
{
    std::mt19937 rng(0);
    std::uniform_int_distribution dist(0, 65535);

    for(size_t n = 0; n < memorySize; n += 2) {
        const auto value = dist(rng);
        memory.WriteWord(n, value);
        EXPECT_EQ(value, memory.ReadWord(n)) << "address " << n;
        EXPECT_EQ(value & 0xff, memory.ReadByte(n + 0)) << "address " << n;
        EXPECT_EQ(value >> 8, memory.ReadByte(n + 1)) << "address " << n;
    }
}

TEST_F(MemoryTest, ResetClearsAllMemoryToZero)
{
    std::mt19937 rng(0);
    std::uniform_int_distribution dist(0, 255);

    for(size_t n = 0; n < memorySize; ++n) {
        const auto value = dist(rng);
        memory.WriteByte(n, value);
        EXPECT_EQ(value, memory.ReadByte(n)) << "address " << n;
    }

    memory.Reset();
    for(size_t n = 0; n < memorySize; ++n) {
        EXPECT_EQ(memory.ReadByte(n), 0) << "address " << n;
    }
}

TEST_F(MemoryTest, GetPointerCanBeReadFrom)
{
    std::mt19937 rng(0);
    std::uniform_int_distribution dist(0, 255);

    for(size_t n = 0; n < memorySize; ++n) {
        const auto value = dist(rng);
        memory.WriteByte(n, value);
        const auto ptr = static_cast<uint8_t*>(memory.GetPointer(n, 1));
        ASSERT_TRUE(ptr);
        EXPECT_EQ(value, *ptr) << "address " << n;
    }
}

TEST_F(MemoryTest, GetPointerCanBeWrittenTo)
{
    std::mt19937 rng(0);
    std::uniform_int_distribution dist(0, 255);

    for(size_t n = 0; n < memorySize; ++n) {
        const auto value = dist(rng);
        const auto ptr = static_cast<uint8_t*>(memory.GetPointer(n, 1));
        ASSERT_TRUE(ptr);
        *ptr = value;
        EXPECT_EQ(value, memory.ReadByte(n)) << "address " << n;
    }
}

TEST_F(MemoryTest, PeripheralsClaimMemorySpace)
{
    MockPeripheral peripheral;
    memory.AddPeripheral(testPeriphalBase, testPeriphalSize, peripheral);

    EXPECT_NE(nullptr, memory.GetPointer(0, testPeriphalBase));
    EXPECT_EQ(nullptr, memory.GetPointer(testPeriphalBase, testPeriphalSize));
    EXPECT_NE(nullptr, memory.GetPointer(testPeriphalBase + testPeriphalSize, 1));
}

TEST_F(MemoryTest, AccessesAreRedirectedToThePeripherals)
{
    MockPeripheral peripheral;
    memory.AddPeripheral(testPeriphalBase, testPeriphalSize, peripheral);

    EXPECT_CALL(peripheral, ReadByte(testPeriphalBase))
        .Times(2)
        .WillOnce(Return(0x10))
        .WillRepeatedly(Return(0x20));

    EXPECT_CALL(peripheral, ReadWord(testPeriphalBase + 0x10))
        .Times(2)
        .WillOnce(Return(0x55aa))
        .WillRepeatedly(Return(0xaa55));

    EXPECT_CALL(peripheral, WriteByte(testPeriphalBase + 0x20, 0x99));
    EXPECT_CALL(peripheral, WriteWord(testPeriphalBase + 0x30, 0xabcd));

    EXPECT_EQ(0x10, memory.ReadByte(testPeriphalBase));
    EXPECT_EQ(0x20, memory.ReadByte(testPeriphalBase));
    EXPECT_EQ(0x55aa, memory.ReadWord(testPeriphalBase + 0x10));
    EXPECT_EQ(0xaa55, memory.ReadWord(testPeriphalBase + 0x10));

    memory.WriteByte(testPeriphalBase + 0x20, 0x99);
    memory.WriteWord(testPeriphalBase + 0x30, 0xabcd);
}

TEST_F(MemoryTest, PeripheralMemoryRangeIsCorrect)
{
    MockPeripheral peripheral;
    memory.AddPeripheral(testPeriphalBase, testPeriphalSize, peripheral);

    using ::testing::_;
    EXPECT_CALL(peripheral, ReadByte(_))
        .Times(0);
    EXPECT_CALL(peripheral, WriteByte(_, _))
        .Times(0);
    EXPECT_CALL(peripheral, ReadWord(_))
        .Times(0);
    EXPECT_CALL(peripheral, WriteWord(_, _))
        .Times(0);

    EXPECT_EQ(0, memory.ReadByte(testPeriphalBase - 1));
    EXPECT_EQ(0, memory.ReadWord(testPeriphalBase - 2));
    EXPECT_EQ(0, memory.ReadByte(testPeriphalBase + testPeriphalSize));
    EXPECT_EQ(0, memory.ReadWord(testPeriphalBase + testPeriphalSize));
    memory.WriteByte(testPeriphalBase - 1, 0xff);
    memory.WriteWord(testPeriphalBase - 2, 0xffff);
    memory.WriteByte(testPeriphalBase + testPeriphalSize, 0xff);
    memory.WriteWord(testPeriphalBase + testPeriphalSize, 0xffff);
}

// TODO: Determine whether this is worth it. Perhaps we should remove the
// *Word() access altogether?
TEST_F(MemoryTest, DISABLED_PeriphalMemoryPartialBoundaryAccess)
{
    MockPeripheral peripheral;
    memory.AddPeripheral(testPeriphalBase, testPeriphalSize, peripheral);

    using ::testing::_;
    EXPECT_CALL(peripheral, ReadByte(testPeriphalBase - 1))
        .Times(1);
    EXPECT_CALL(peripheral, ReadWord(_))
        .Times(0);

    EXPECT_EQ(0, memory.ReadWord(testPeriphalBase - 1));
}