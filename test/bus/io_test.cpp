#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "bus/io.h"

using ::testing::Return;

namespace
{
    constexpr inline size_t ioSize = 65536;
    constexpr inline io_port testPeriphalBase = 0x1'000;
    constexpr inline size_t testPeriphalSize = 64;

    struct MockPeripheral : IOPeripheral
    {
        MOCK_METHOD(void, Out8, (io_port port, uint8_t val), (override));
        MOCK_METHOD(void, Out16, (io_port port, uint16_t val), (override));

        MOCK_METHOD(uint8_t, In8, (io_port port), (override));
        MOCK_METHOD(uint16_t, In16, (io_port port), (override));
    };

    struct IOTest : ::testing::Test
    {
        IO io;
    };
}

TEST_F(IOTest, Instantiation)
{
}

TEST_F(IOTest, UnmappedPortsReadAsZero)
{
    for(size_t n = 0; n < ioSize; ++n) {
        EXPECT_EQ(0, io.In8(n)) << "io " << n;
    }

    for(size_t n = 0; n < ioSize; n += 2) {
        EXPECT_EQ(0, io.In16(n)) << "io " << n;
    }
}

TEST_F(IOTest, AccessesAreRedirectedToThePeripherals)
{
    MockPeripheral peripheral;
    io.AddPeripheral(testPeriphalBase, testPeriphalSize, peripheral);

    EXPECT_CALL(peripheral, In8(testPeriphalBase))
        .Times(2)
        .WillOnce(Return(0xa0))
        .WillRepeatedly(Return(0xb0));

    EXPECT_CALL(peripheral, In16(testPeriphalBase + 0x10))
        .Times(2)
        .WillOnce(Return(0xc0d0))
        .WillRepeatedly(Return(0xe0f0));

    EXPECT_CALL(peripheral, Out8(testPeriphalBase + 0x20, 0x99));
    EXPECT_CALL(peripheral, Out16(testPeriphalBase + 0x30, 0xabcd));

    EXPECT_EQ(0xa0, io.In8(testPeriphalBase));
    EXPECT_EQ(0xb0, io.In8(testPeriphalBase));
    EXPECT_EQ(0xc0d0, io.In16(testPeriphalBase + 0x10));
    EXPECT_EQ(0xe0f0, io.In16(testPeriphalBase + 0x10));

    io.Out8(testPeriphalBase + 0x20, 0x99);
    io.Out16(testPeriphalBase + 0x30, 0xabcd);
}

TEST_F(IOTest, PeripheralIoRangeIsCorrect)
{
    MockPeripheral peripheral;
    io.AddPeripheral(testPeriphalBase, testPeriphalSize, peripheral);

    using ::testing::_;
    EXPECT_CALL(peripheral, In8(_))
        .Times(0);
    EXPECT_CALL(peripheral, Out8(_, _))
        .Times(0);
    EXPECT_CALL(peripheral, In16(_))
        .Times(0);
    EXPECT_CALL(peripheral, Out16(_, _))
        .Times(0);

    EXPECT_EQ(0, io.In8(testPeriphalBase - 1));
    EXPECT_EQ(0, io.In16(testPeriphalBase - 2));
    EXPECT_EQ(0, io.In8(testPeriphalBase + testPeriphalSize));
    EXPECT_EQ(0, io.In16(testPeriphalBase + testPeriphalSize));
    io.Out8(testPeriphalBase - 1, 0xff);
    io.Out16(testPeriphalBase - 2, 0xffff);
    io.Out8(testPeriphalBase + testPeriphalSize, 0xff);
    io.Out16(testPeriphalBase + testPeriphalSize, 0xffff);
}

// TODO: Reconsider 16-bit I/O access (does that even exist on x86 hardware?)