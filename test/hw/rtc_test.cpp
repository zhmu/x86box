#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "bus/io.h"
#include "hw/rtc.h"
#include "interface/timeinterface.h"

using ::testing::Return;

namespace
{
    struct TimeMock : TimeInterface
    {
        MOCK_METHOD(LocalTime, GetLocalTime, (), (override));
    };

    struct RTCTest : ::testing::Test
    {
        IO io;
        TimeMock time;
        RTC rtc;

        RTCTest() : rtc(io, time) { }
    };

    auto ReadRegister(IOInterface& io, uint8_t reg)
    {
        io.Out8(0x70, reg);
        return io.In8(0x71);
    }
}

TEST_F(RTCTest, Instantiation)
{
}

TEST_F(RTCTest, RegistersAreZeroByDefault)
{
    for(int n = 0; n < 0x2f; ++n) {
        if (n < 10 || n == 0x10 /* floppy */ || n == 0x32 /* century */) continue;
        EXPECT_EQ(0, ReadRegister(io, n)) << "register " << n;
    }
}

TEST_F(RTCTest, FloppyValueIsProperlySet)
{
    const auto value = ReadRegister(io, 0x10);
    EXPECT_EQ(0x4, value >> 4); // first floppy
    EXPECT_EQ(0x0, value & 0xf); // second floppy
}

TEST_F(RTCTest, TimeIsProperlyReturned)
{
    constexpr LocalTime t{
        .seconds = 42, .minutes = 21, .hours = 18,
        .week_day = 1, .day = 6, .month = 8, .year = 2023
    };

    EXPECT_CALL(time, GetLocalTime())
        .WillRepeatedly(Return(t));

    EXPECT_EQ(0x42, ReadRegister(io, 0x00));
    EXPECT_EQ(0x21, ReadRegister(io, 0x02));
    EXPECT_EQ(0x18, ReadRegister(io, 0x04));
    EXPECT_EQ(0x01, ReadRegister(io, 0x06));
    EXPECT_EQ(0x06, ReadRegister(io, 0x07));
    EXPECT_EQ(0x08, ReadRegister(io, 0x08));
    EXPECT_EQ(0x23, ReadRegister(io, 0x09));
    EXPECT_EQ(0x20, ReadRegister(io, 0x32));
}