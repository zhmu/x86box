#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "interface/memoryinterface.h"
#include "hw/dma.h"
#include "bus/io.h"
#include <array>

namespace
{
    constexpr inline io_port Pic1Data = 0x21;
    constexpr inline uint8_t unmaskEverything = 0x00;
    constexpr inline uint8_t maskEverything = 0xff;

    constexpr inline uint32_t dmaAddress = 0x1000;
    constexpr inline size_t dmaCount = 16;

    // OEIS A000108
    constexpr auto dummy_data = std::to_array<uint8_t>({
        0x11, 0x25, 0x14, 0x42, 0x13, 0x24, 0x29, 0x14,
        0x30, 0x48, 0x62, 0x16, 0x79, 0x65, 0x87, 0x86});
    static_assert(dummy_data.size() == dmaCount);

    struct MockMemory : MemoryInterface
    {
        MOCK_METHOD(uint8_t, ReadByte, (memory::Address addr), (override));
        MOCK_METHOD(uint16_t, ReadWord, (memory::Address addr), (override));

        MOCK_METHOD(void, WriteByte, (memory::Address addr, uint8_t data), (override));
        MOCK_METHOD(void, WriteWord, (memory::Address addr, uint16_t data), (override));

        MOCK_METHOD(void, AddPeripheral, (memory::Address base, uint16_t length, MemoryMappedPeripheral& peripheral), (override));

        MOCK_METHOD(void*, GetPointer, (memory::Address addr, uint16_t length), (override));
    };

    struct DMATest : ::testing::Test
    {
        MockMemory memory;
        IO io;
        DMA dma;

        DMATest() : dma(io, memory) { }
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
}

TEST_F(DMATest, Instantiation)
{
}

TEST_F(DMATest, PeripheralToMemoryTransfersTheCorrectData)
{
    using ::testing::Sequence;

    Sequence s;
    for(size_t n = 0; n < dummy_data.size(); ++n) {
        EXPECT_CALL(memory, WriteByte(dmaAddress + n, dummy_data[n]))
            .InSequence(s);
    }
    
    SetupDMATransfer(io);
    auto xfer = dma.InitiateTransfer(2);
    EXPECT_EQ(dmaCount, xfer->GetTotalLength());

    const auto length = xfer->WriteFromPeripheral(0, dummy_data);
    EXPECT_EQ(dummy_data.size(), length);
}

// TODO We need to more tests to verify the status bits etc