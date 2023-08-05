#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "interface/dmainterface.h"
#include "interface/picinterface.h"
#include "interface/imageprovider.h"
#include "hw/ata.h"
#include "bus/io.h"

namespace
{
    using SectorData = std::array<uint8_t, 512>;

    namespace io
    {
        // XT-IDE
        static constexpr inline io_port Base = 0x300;
        constexpr auto GetRegisterOffset(int x) { return x * 2; }

        //
        static constexpr inline io_port Data = Base + GetRegisterOffset(0x0);
        static constexpr inline io_port Error_Read = Base + GetRegisterOffset(0x1);
        static constexpr inline io_port Feature_Write = Base + GetRegisterOffset(0x1);
        static constexpr inline io_port SectorCount = Base + GetRegisterOffset(0x2);
        static constexpr inline io_port SectorNumber = Base + GetRegisterOffset(0x3);
        static constexpr inline io_port CylinderLow = Base + GetRegisterOffset(0x4);
        static constexpr inline io_port CylinderHigh = Base + GetRegisterOffset(0x5);
        static constexpr inline io_port DriveHead = Base + GetRegisterOffset(0x6);

        // 3f6 for primary IDE
        static constexpr inline io_port AltStatus = Base + GetRegisterOffset(0x7);
        static constexpr inline io_port DevControl = Base + GetRegisterOffset(0x7);
    }

    struct MockImageProvider : ImageProvider
    {
        MOCK_METHOD(size_t, Read, (const Image image, uint64_t offset, std::span<uint8_t> data), (override));
        MOCK_METHOD(size_t, Write, (const Image image, uint64_t offset, std::span<const uint8_t> data), (override));
    };

    struct ATATest : ::testing::Test
    {
        IO io;
        MockImageProvider imageProvider;
        ATA ata;

        ATATest() : ata(io, imageProvider) { }
    };

    auto DecodeAsU16(const std::uint8_t* p)
    {
        const auto lo = static_cast<uint16_t>(p[0]);
        const auto hi = static_cast<uint16_t>(p[1]);
        return (hi << 8) | lo;
    }

    auto ReadSector(IOInterface& io)
    {
        SectorData data;
        for(auto& value: data)
            value = io.In8(io::Data);
        return data;
    }

    void WriteSector(IOInterface& io, const SectorData& data)
    {
        for(auto value: data)
            io.Out8(io::Data, value);
    }

    auto DecodeString(std::span<const uint8_t> p)
    {
        std::string s;

        // Byte-swap the content from the buffer
        for(size_t n = 0; n < p.size(); n += 2) {
            s += p[n + 1];
            s += p[n + 0];
        }

        if(!s.empty()) {
            // Remove trailing ' '
            auto it = std::prev(s.end());
            while (it != s.begin() && *it == ' ')
                --it;
            s.erase(std::next(it), s.end());
        }
        return s;
    }
}

TEST_F(ATATest, Instantiation)
{
}

TEST_F(ATATest, Identify)
{
    io.Out8(io::DriveHead, 0xa0);
    io.Out8(io::SectorCount, 0);
    io.Out8(io::CylinderHigh, 0);
    io.Out8(io::CylinderLow, 0);
    io.Out8(io::SectorNumber, 0);
    io.Out8(io::DevControl, 0xec); // identify
    EXPECT_EQ(0b1001000, io.In8(io::AltStatus)); // READY, DATA REQ

    const auto data = ReadSector(io);
    EXPECT_EQ(0b1000'0000, data[1]); // fixed device
    EXPECT_EQ(615, DecodeAsU16(&data[2])); // cylinders
    EXPECT_EQ(6, DecodeAsU16(&data[6])); // heads
    EXPECT_EQ(17, DecodeAsU16(&data[12])); // sectors per track
    EXPECT_EQ("DUMMY DRIVE", DecodeString({ &data[54], &data[92] })); // model
    EXPECT_EQ(0b1000000, io.In8(io::AltStatus)); // READY
}

TEST_F(ATATest, ReadSectorsReadsOneSector)
{
    using ::testing::Return;
    using ::testing::_;
    EXPECT_CALL(imageProvider, Read(Image::Harddisk0, 0, _))
        .Times(1)
        .WillOnce(Return(512));

    io.Out8(io::DriveHead, 0xa0);
    io.Out8(io::SectorCount, 1);
    io.Out8(io::CylinderHigh, 0);
    io.Out8(io::CylinderLow, 0);
    io.Out8(io::SectorNumber, 1);
    io.Out8(io::DevControl, 0x20); // read sectors
    EXPECT_EQ(0b1001000, io.In8(io::AltStatus)); // READY, DATA REQ

    const auto data = ReadSector(io);
    // TODO verify the data read
    EXPECT_EQ(0b1000000, io.In8(io::AltStatus)); // READY
}

TEST_F(ATATest, ReadSectorsReadsMultipleSectors)
{
    constexpr auto numberOfSectors = 3;

    using ::testing::Sequence;
    using ::testing::Return;
    using ::testing::_;

    Sequence s;
    for(int n = 0; n < numberOfSectors; ++n) {
        EXPECT_CALL(imageProvider, Read(Image::Harddisk0, n * 512, _))
            .Times(1)
            .InSequence(s)
            .WillOnce(Return(512));
    }

    io.Out8(io::DriveHead, 0xa0);
    io.Out8(io::SectorCount, numberOfSectors);
    io.Out8(io::CylinderHigh, 0);
    io.Out8(io::CylinderLow, 0);
    io.Out8(io::SectorNumber, 1);
    io.Out8(io::DevControl, 0x20); // read sectors
    EXPECT_EQ(0b1001000, io.In8(io::AltStatus)); // READY, DATA REQ

    for(int n = 0; n < numberOfSectors; ++n) {
        const auto data = ReadSector(io);
        // TODO verify the data read
    }
    EXPECT_EQ(0b1000000, io.In8(io::AltStatus)); // READY
}

TEST_F(ATATest, WriteSectorsWritesOneSector)
{
    using ::testing::Return;
    using ::testing::_;
    EXPECT_CALL(imageProvider, Write(Image::Harddisk0, 0, _))
        .Times(1)
        .WillOnce(Return(512));

    io.Out8(io::DriveHead, 0xa0);
    io.Out8(io::SectorCount, 1);
    io.Out8(io::CylinderHigh, 0);
    io.Out8(io::CylinderLow, 0);
    io.Out8(io::SectorNumber, 1);
    io.Out8(io::DevControl, 0x30); // write sectors
    EXPECT_EQ(0b1001000, io.In8(io::AltStatus)); // READY, DATA REQ

    SectorData data{};
    WriteSector(io, data);
    EXPECT_EQ(0b1000000, io.In8(io::AltStatus)); // READY
}

TEST_F(ATATest, WriteSectorsWritesMultipleSectors)
{
    constexpr auto numberOfSectors = 3;

    using ::testing::Sequence;
    using ::testing::Return;
    using ::testing::_;

    Sequence s;
    for(int n = 0; n < numberOfSectors; ++n) {
        EXPECT_CALL(imageProvider, Write(Image::Harddisk0, n * 512, _))
            .Times(1)
            .InSequence(s)
            .WillOnce(Return(512));
            // TODO verify the data written
    }

    io.Out8(io::DriveHead, 0xa0);
    io.Out8(io::SectorCount, numberOfSectors);
    io.Out8(io::CylinderHigh, 0);
    io.Out8(io::CylinderLow, 0);
    io.Out8(io::SectorNumber, 1);
    io.Out8(io::DevControl, 0x30); // write sectors
    EXPECT_EQ(0b1001000, io.In8(io::AltStatus)); // READY, DATA REQ

    for(int n = 0; n < numberOfSectors; ++n) {
        SectorData data{};
        WriteSector(io, data);
    }
    EXPECT_EQ(0b1000000, io.In8(io::AltStatus)); // READY
}