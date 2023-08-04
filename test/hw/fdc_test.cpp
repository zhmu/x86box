#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "interface/dmainterface.h"
#include "interface/picinterface.h"
#include "interface/imageprovider.h"
#include "hw/fdc.h"
#include "bus/io.h"
#include <array>

namespace
{
    constexpr inline uint32_t dmaAddress = 0x1000;
    constexpr inline size_t dmaCount = 16;

    // OEIS A000108
    constexpr auto dummy_data = std::to_array<uint8_t>({
        0x11, 0x25, 0x14, 0x42, 0x13, 0x24, 0x29, 0x14,
        0x30, 0x48, 0x62, 0x16, 0x79, 0x65, 0x87, 0x86});
    static_assert(dummy_data.size() == dmaCount);

    struct MockPIC : PICInterface
    {
        MOCK_METHOD(void, AssertIRQ, (IRQ irq), (override));
        MOCK_METHOD(void, SetPendingIRQState, (IRQ irq, bool pending), (override));
        MOCK_METHOD(std::optional<int>, DequeuePendingIRQ, (), (override));
    };

    struct MockDMATransfer : DMATransfer
    {
        MOCK_METHOD(size_t, WriteFromPeripheral, (uint16_t offset, std::span<const uint8_t> data), (override));
        MOCK_METHOD(size_t, GetTotalLength, (), (override));
        MOCK_METHOD(void, Complete, (), (override));
    };

    struct MockDMA : DMAInterface
    {
        MOCK_METHOD(std::unique_ptr<DMATransfer>, InitiateTransfer, (int ch_num), (override));
    };

    struct MockImageProvider : ImageProvider
    {
        MOCK_METHOD(size_t, Read, (const Image image, uint64_t offset, std::span<uint8_t> data), (override));
        MOCK_METHOD(size_t, Write, (const Image image, uint64_t offset, std::span<const uint8_t> data), (override));
    };

    struct FDCTest : ::testing::Test
    {
        IO io;
        MockPIC pic;
        MockDMA dma;
        MockImageProvider imageProvider;
        FDC fdc;

        FDCTest() : fdc(io, pic, dma, imageProvider) { }
    };

    void SetupDMATransfer(IOInterface& io)
    {
        io.Out8(0x0a, 0x06); // mask channel 2
        io.Out8(0x0b, 0x46); // mode: single, addr increment, write
        io.Out8(0x0c, 0xff); // reset master flip-flop
        io.Out8(0x04, (dmaAddress & 0xff)); // address, bits 7..0
        io.Out8(0x04, (dmaAddress >> 8) & 0xff); // address, bits 15..8
        io.Out8(0x0c, 0xff); // reset master flip-flop
        io.Out8(0x05, (dmaCount - 1) & 0xff); // count, bits 7..0
        io.Out8(0x05, (dmaCount - 1) >> 8); // count, bits 15..8
        io.Out8(0x81, (dmaAddress >> 16) & 0xff); // address, bits 23..16
        io.Out8(0x0a, 0x02); // unmask channel 2
    }

    std::pair<uint8_t, uint8_t> IssueSenseInterruptStatus(IOInterface& io)
    {
        io.Out8(0x3f5, 0x08); // command: sense interrupt status
        const auto st0 = io.In8(0x3f5);
        const auto pcn = io.In8(0x3f5);
        return { st0, pcn };
    }

    void IssueSeek(IOInterface& io)
    {
        io.Out8(0x3f5, 0x0f); // command: seek
        io.Out8(0x3f5, 0x00); // hds
        io.Out8(0x3f5, 0x00); // ncn
    }

    struct ExtendedStatus {
        uint8_t st0, st1, st2, c, h, r, n;
    };

    ExtendedStatus RetrieveExtendedStatus(IOInterface& io)
    {
        const auto st0 = io.In8(0x3f5);
        const auto st1 = io.In8(0x3f5);
        const auto st2 = io.In8(0x3f5);
        const auto c = io.In8(0x3f5);
        const auto h = io.In8(0x3f5);
        const auto r = io.In8(0x3f5);
        const auto n = io.In8(0x3f5);
        return { st0, st1, st2, c, h, r, n };
    }

    ExtendedStatus IssueReadData(IOInterface& io)
    {
        io.Out8(0x3f5, 0x06); // command: read data
        io.Out8(0x3f5, 0x00); // hds/ds1/ds0
        io.Out8(0x3f5, 0x00); // c
        io.Out8(0x3f5, 0x00); // h
        io.Out8(0x3f5, 0x01); // r (sector)
        io.Out8(0x3f5, 0x00); // n
        io.Out8(0x3f5, 0x00); // eot
        io.Out8(0x3f5, 0x00); // gpl
        io.Out8(0x3f5, 0x00); // dtl
        return RetrieveExtendedStatus(io);
    }
}

TEST_F(FDCTest, Instantiation)
{
}

TEST_F(FDCTest, ResetTriggersIRQ)
{
    EXPECT_CALL(pic, AssertIRQ(PICInterface::IRQ::FDC))
        .Times(1);

    io.Out8(0x3f2, 0x0); // /RESET -> 1
    io.Out8(0x3f2, 0x4); // /RESET -> 0
}

TEST_F(FDCTest, UnimplementedCommandDoesNothing)
{
    io.Out8(0x3f5, 0x99);
}

TEST_F(FDCTest, SenseInterruptStatus)
{
    const auto [ st0, pcn ] = IssueSenseInterruptStatus(io);
    EXPECT_EQ(0b1100'0000, st0); // IC1 | IC0
    EXPECT_EQ(0, pcn);
}

TEST_F(FDCTest, Specify)
{
    io.Out8(0x3f5, 0x03); // command: specify
    io.Out8(0x3f5, 0x00); // srt/hut
    io.Out8(0x3f5, 0x00); // hlt/nd
}

TEST_F(FDCTest, Recalibrate)
{
    EXPECT_CALL(pic, AssertIRQ(PICInterface::IRQ::FDC))
        .Times(1);

    io.Out8(0x3f5, 0x07); // command: recalibrate
    io.Out8(0x3f5, 0x00); // ds
}

TEST_F(FDCTest, Seek)
{
    EXPECT_CALL(pic, AssertIRQ(PICInterface::IRQ::FDC))
        .Times(1);

    IssueSeek(io);
}

TEST_F(FDCTest, ReadID)
{
    EXPECT_CALL(pic, AssertIRQ(PICInterface::IRQ::FDC))
        .Times(1);

    io.Out8(0x3f5, 0x0a); // command: read id
    io.Out8(0x3f5, 0x00); // hds/ds1/ds0
    const auto [ st0, st1, st2, c, h, r, n ] = RetrieveExtendedStatus(io);
    EXPECT_EQ(0b1100'0000, st0); // ic1/ic0
    EXPECT_EQ(0, st1);
    EXPECT_EQ(0, st2);
    EXPECT_EQ(0, c); // cylinder
    EXPECT_EQ(0, h); // head address
    EXPECT_EQ(0, r); // sector address
    EXPECT_EQ(2, n); // sector size (512)
}

TEST_F(FDCTest, DiskChanged)
{
    EXPECT_CALL(pic, AssertIRQ(PICInterface::IRQ::FDC))
        .Times(1); // Seek

    // Initially, DC should not be set
    const auto dir1 = io.In8(0x3f7);
    EXPECT_EQ(0, dir1);
    // Changing the image should trigger the DC flag
    fdc.NotifyImageChanged();
    const auto dir2 = io.In8(0x3f7);
    EXPECT_EQ(0b1000'0000, dir2);
    // Seek should reset the DC flag
    IssueSeek(io);
    const auto dir3 = io.In8(0x3f7);
    EXPECT_EQ(0, dir3);
}

TEST_F(FDCTest, ReadDataReportsErrorsOnReadFailure)
{
    using ::testing::Return;
    using ::testing::_;
    auto transfer = std::make_unique<MockDMATransfer>();

    EXPECT_CALL(*transfer, GetTotalLength())
        .Times(1)
        .WillOnce(Return(512));
    EXPECT_CALL(*transfer, Complete())
        .Times(1);

    EXPECT_CALL(dma, InitiateTransfer(2))
        .Times(1)
        .WillOnce(Return(std::move(transfer)));

    EXPECT_CALL(pic, AssertIRQ(PICInterface::IRQ::FDC))
        .Times(1);

    EXPECT_CALL(imageProvider, Read(Image::Floppy0, 0, _))
        .Times(1)
        .WillOnce(Return(0));

    const auto [ st0, st1, st2, c, h, r, n ] = IssueReadData(io);
    EXPECT_EQ(0b0100'0000, st0); // ic0
    EXPECT_EQ(0b100, st1); // ND
    EXPECT_EQ(0, st2);
    EXPECT_EQ(0, c); // cylinder
    EXPECT_EQ(0, h); // head address
    EXPECT_EQ(1, r); // sector address
    EXPECT_EQ(0, n); // sector size (512)
}

TEST_F(FDCTest, ReadDataTransfersData)
{
    using ::testing::Return;
    using ::testing::_;
    auto transfer = std::make_unique<MockDMATransfer>();

    EXPECT_CALL(*transfer, GetTotalLength())
        .Times(1)
        .WillOnce(Return(512));
    EXPECT_CALL(*transfer, Complete())
        .Times(1);
    EXPECT_CALL(*transfer, WriteFromPeripheral(0, _))
        .Times(1)
        .WillOnce(Return(512));

    EXPECT_CALL(dma, InitiateTransfer(2))
        .Times(1)
        .WillOnce(Return(std::move(transfer)));

    EXPECT_CALL(pic, AssertIRQ(PICInterface::IRQ::FDC))
        .Times(1);

    EXPECT_CALL(imageProvider, Read(Image::Floppy0, 0, _))
        .Times(1)
        .WillOnce(Return(512));

    const auto [ st0, st1, st2, c, h, r, n ] = IssueReadData(io);
    EXPECT_EQ(0b1100'0000, st0); // ic1/ic0
    EXPECT_EQ(0, st1);
    EXPECT_EQ(0, st2);
    EXPECT_EQ(0, c); // cylinder
    EXPECT_EQ(0, h); // head address
    EXPECT_EQ(1, r); // sector address
    EXPECT_EQ(0, n); // sector size (512)

    // TODO: We need to verify that the correct data was transferred!
}