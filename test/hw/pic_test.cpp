#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "hw/pic.h"
#include "bus/io.h"

namespace
{
    constexpr inline io_port Pic1Data = 0x21;
    constexpr inline uint8_t unmaskEverything = 0x00;
    constexpr inline uint8_t maskEverything = 0xff;

    constexpr auto EnableIRQ(PICInterface::IRQ irq, uint8_t mask = maskEverything)
    {
        mask &= ~(1 << static_cast<uint8_t>(irq));
        return mask;
    }

    struct PICTest : ::testing::Test
    {
        IO io;
        PIC pic;

        PICTest() : pic(io) { }
    };
}

TEST_F(PICTest, Instantiation)
{
}

TEST_F(PICTest, InitiallyIRQsAreMasked)
{
    EXPECT_EQ(maskEverything, io.In8(Pic1Data));

    pic.AssertIRQ(PICInterface::IRQ::PIT);
    const auto pendingIrq = pic.DequeuePendingIRQ();
    EXPECT_FALSE(pendingIrq);
}

TEST_F(PICTest, MaskCanBeSet)
{
    for (auto mask: { EnableIRQ(PICInterface::IRQ::PIT) }) {
        io.Out8(Pic1Data, mask);
        EXPECT_EQ(mask, io.In8(Pic1Data));
    }
}

TEST_F(PICTest, UnmaskingANonPendingIRQDoesNothing)
{
    io.Out8(Pic1Data, EnableIRQ(PICInterface::IRQ::PIT));
    pic.AssertIRQ(PICInterface::IRQ::Keyboard);
    const auto pendingIrq = pic.DequeuePendingIRQ();
    EXPECT_FALSE(pendingIrq);
}

TEST_F(PICTest, UnmaskingAPendingIRQTriggersIt)
{
    io.Out8(Pic1Data, EnableIRQ(PICInterface::IRQ::PIT));
    pic.AssertIRQ(PICInterface::IRQ::PIT);
    const auto pendingIrq = pic.DequeuePendingIRQ();
    ASSERT_TRUE(pendingIrq);
    EXPECT_EQ(static_cast<int>(PICInterface::IRQ::PIT), *pendingIrq);
}

TEST_F(PICTest, ChangingTheMaskDoesNotResetAPendingInterrupt)
{
    pic.AssertIRQ(PICInterface::IRQ::PIT);
    const auto pendingIrq1 = pic.DequeuePendingIRQ();
    ASSERT_FALSE(pendingIrq1);

    io.Out8(Pic1Data, EnableIRQ(PICInterface::IRQ::Keyboard));
    const auto pendingIrq2 = pic.DequeuePendingIRQ();
    ASSERT_FALSE(pendingIrq2);

    io.Out8(Pic1Data, EnableIRQ(PICInterface::IRQ::PIT));
    const auto pendingIrq3 = pic.DequeuePendingIRQ();
    ASSERT_TRUE(pendingIrq3);
    EXPECT_EQ(static_cast<int>(PICInterface::IRQ::PIT), *pendingIrq3);
}

TEST_F(PICTest, PendingIRQsAreTriggeredFromHighestToLowestPriority)
{
    io.Out8(Pic1Data, unmaskEverything);
    for(unsigned int n = 0; n < 8; ++n)
        pic.AssertIRQ(static_cast<PICInterface::IRQ>(n));

    for(unsigned int n = 0; n < 8; ++n) {
        const auto pendingIrq = pic.DequeuePendingIRQ();
        ASSERT_TRUE(pendingIrq);
        EXPECT_EQ(n, *pendingIrq);
    }

    const auto pendingIrq = pic.DequeuePendingIRQ();
    ASSERT_FALSE(pendingIrq);
}
