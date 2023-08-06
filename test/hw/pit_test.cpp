#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "hw/pit.h"
#include "bus/io.h"
#include "interface/tickinterface.h"

using namespace std::literals::chrono_literals; // ns
using ::testing::Return;

namespace
{
    struct MockTick : TickInterface
    {
        MOCK_METHOD(std::chrono::nanoseconds, GetTickCount, (), (override));
    };

    struct PITTest : ::testing::Test
    {
        IO io;
        MockTick tick;
        PIT pit;

        PITTest() : pit(io, tick) { }
    };

    void SetChannel0SquareWave(IOInterface& io)
    {
        io.Out8(0x43, 0x36); // channel 0: lsb & msb, mode 3, binary
        io.Out8(0x40, 0x00); // count low byte
        io.Out8(0x40, 0x00); // count hi byte
    }
}

TEST_F(PITTest, Instantiation)
{
}

TEST_F(PITTest, SquareWaveTriggersImmediatelyAfterSetting)
{
    EXPECT_CALL(tick, GetTickCount())
        .Times(2)
        .WillRepeatedly(Return(100ns));

    SetChannel0SquareWave(io);
    EXPECT_EQ(pit.Tick(), true);
}

TEST_F(PITTest, SquareWaveWithMaxCountTriggersEvery55ms)
{
    // 55ms / 2 = 27.5ms = 27500us
    EXPECT_CALL(tick, GetTickCount())
        .Times(6)
        .WillOnce(Return(0 * 27'500us))
        .WillOnce(Return(1 * 27'500us))
        .WillOnce(Return(2 * 27'500us))
        .WillOnce(Return(3 * 27'500us))
        .WillOnce(Return(4 * 27'500us))
        .WillOnce(Return(5 * 27'500us));

    SetChannel0SquareWave(io);
    EXPECT_EQ(pit.Tick(), false);
    EXPECT_EQ(pit.Tick(), true);
    EXPECT_EQ(pit.Tick(), false);
    EXPECT_EQ(pit.Tick(), true);
    EXPECT_EQ(pit.Tick(), false);
}