#include "dma.h"
#include "io.h"
#include "memory.h"

#include "spdlog/spdlog.h"

// Intel 8237
namespace
{
    namespace io
    {
        constexpr inline io_port Base = 0x0;

        constexpr inline io_port Ch0_CurrentAddress_Read = Base + 0x0;
        constexpr inline io_port Ch0_BaseAddress_Write = Base + 0x0;
        constexpr inline io_port Ch0_WordCount = Base + 0x1;
        constexpr inline io_port Ch1_CurrentAddress_Read = Base + 0x2;
        constexpr inline io_port Ch1_BaseAddress_Write = Base + 0x2;
        constexpr inline io_port Ch1_WordCount = Base + 0x3;
        constexpr inline io_port Ch2_CurrentAddress_Read = Base + 0x4;
        constexpr inline io_port Ch2_BaseAddress_Write = Base + 0x4;
        constexpr inline io_port Ch2_WordCount = Base + 0x5;
        constexpr inline io_port Ch3_CurrentAddress_Read = Base + 0x6;
        constexpr inline io_port Ch3_BaseAddress_Write = Base + 0x6;
        constexpr inline io_port Ch3_WordCount = Base + 0x7;

        constexpr inline io_port Status_Read = Base + 0x8;
        constexpr inline io_port Command_Write = Base + 0x8;
        constexpr inline io_port WriteRequest_Write = Base + 0x9;
        constexpr inline io_port Mask = Base + 0xa;
        constexpr inline io_port Mode = Base + 0xb;

        constexpr inline io_port ClearByte_Write = Base + 0xc;
        constexpr inline io_port Temp_Read = Base + 0xd;
        constexpr inline io_port MasterClear_Write = Base + 0xd;
        constexpr inline io_port ClearMask_Write = Base + 0xe;
        constexpr inline io_port WriteMask = Base + 0xf;

        constexpr inline io_port Ch0_PageAddr = 0x87;
        constexpr inline io_port Ch1_PageAddr = 0x83;
        constexpr inline io_port Ch2_PageAddr = 0x81;
        constexpr inline io_port Ch3_PageAddr = 0x82;
    }

    namespace status {
        constexpr inline uint8_t Ch3RequestActive = (1 << 7);
        constexpr inline uint8_t Ch2RequestActive = (1 << 6);
        constexpr inline uint8_t Ch1RequestActive = (1 << 5);
        constexpr inline uint8_t Ch3TransferComplete = (1 << 3);
        constexpr inline uint8_t Ch2TransferComplete = (1 << 2);
        constexpr inline uint8_t Ch1TransferComplete = (1 << 1);
        constexpr inline uint8_t Ch0TransferComplete = (1 << 0);
    }

    namespace mode {
        constexpr inline uint8_t Ch0Select = 0b00;
        constexpr inline uint8_t Ch1Select = 0b01;
        constexpr inline uint8_t Ch2Select = 0b10;
        constexpr inline uint8_t Ch3Select = 0b11;
        constexpr inline uint8_t ChSelectMask = 0b11;

        constexpr inline uint8_t VerifyTransfer = 0b00 << 2;
        constexpr inline uint8_t WriteTransfer = 0b01 << 2;
        constexpr inline uint8_t ReadTransfer = 0b10 << 2;
        constexpr inline uint8_t TransferMask = 0b11 << 2;

        constexpr inline uint8_t AutoInit = (1 << 4);
        constexpr inline uint8_t Reverse = (1 << 5);

        constexpr inline uint8_t DemandMode = 0b00 << 6;
        constexpr inline uint8_t SingleMode = 0b01 << 6;
        constexpr inline uint8_t BlockMode = 0b10 << 6;
        constexpr inline uint8_t CascadeMode = 0b11 << 6;
        constexpr inline uint8_t ModeMask = 0b11 << 6;
    }
}

struct DMA::Impl : IOPeripheral
{
    Memory& memory;

    struct Channel {
        uint8_t mode{};
        uint8_t status{};
        uint32_t address{};
        uint16_t count{};
    };

    std::array<Channel, 4> channel;
    uint8_t mask{};
    uint8_t status{};
    bool flipflop{};

    Impl(Memory& memory) : memory(memory) { }

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;

    void Reset();
    void WriteDataFromPeriphal(int ch, std::span<const uint8_t> data);
};

DMA::DMA(IO& io, Memory& memory)
    : impl(std::make_unique<Impl>(memory))
{
    io.AddPeripheral(io::Base, 16, *impl);
    io.AddPeripheral(io::Ch0_PageAddr, 1, *impl);
    io.AddPeripheral(io::Ch1_PageAddr, 1, *impl);
    io.AddPeripheral(io::Ch2_PageAddr, 1, *impl);
    io.AddPeripheral(io::Ch3_PageAddr, 1, *impl);
}

DMA::~DMA() = default;

void DMA::Reset()
{
    impl->Reset();
}

void DMA::WriteDataFromPeriphal(int ch, std::span<const uint8_t> data)
{
    impl->WriteDataFromPeriphal(ch, data);
}

void DMA::Impl::Reset()
{
    std::fill(channel.begin(), channel.end(), Channel{});
    status = 0;
    mask = 0xff;
    flipflop = false;
}

void DMA::Impl::Out8(io_port port, uint8_t val)
{
    spdlog::info("dma: out8({:x}, {:x})", port, val);
    switch(port) {
        case io::MasterClear_Write:
            Reset();
            break;
        case io::ClearByte_Write:
            flipflop = false;
            break;
        case io::Ch2_BaseAddress_Write:
            if (flipflop) {
                channel[2].address = channel[2].address | (static_cast<uint32_t>(val) << 8);
            } else {
                channel[2].address = val;
                flipflop = true;
            }
            break;
        case io::Ch2_WordCount:
            if (flipflop) {
                channel[2].count = channel[2].count | (static_cast<uint16_t>(val) << 8);
            } else {
                channel[2].count = val;
                flipflop = true;
            }
            break;
        case io::Ch2_PageAddr:
            channel[2].address = channel[2].address | (static_cast<uint32_t>(val) << 16);
            break;
        case io::Mode: {
            const auto ch = val & 3;
            channel[ch].mode = val;
            break;
        }
        case io::Mask: {
            const auto sel10 = val & 3;
            if ((val & 0b100) != 0) /* MASK_ON */ {
                mask |= (1 << sel10);
                status |= (1 << (4 + sel10)); // TODO is this correct?
            } else {
                mask &= ~(1 << sel10);
                status &= ~(1 << (4 + sel10)); // TODO is this correct?
            }
            break;
        }
    }
}

void DMA::Impl::WriteDataFromPeriphal(int ch_num, std::span<const uint8_t> data)
{
    spdlog::info("dma: write data from periphal, ch {}, length {}", ch_num, data.size());
    if (mask & (1 << ch_num)) {
        spdlog::error("dma: ignoring write data from periphal, ch {}: channel is masked", ch_num);
        return;
    }

    auto& ch = channel[ch_num];
    const size_t data_expected = ch.count + 1;
    if (data.size() != data_expected) {
        spdlog::error("dma: ignoring write data from periphal, ch {}: size mismatch (expected {} got {})", ch_num, data_expected, data.size());
        return;
    }
    const auto transfer = ch.mode & mode::TransferMask;
    if (transfer != mode::WriteTransfer) {
        spdlog::error("dma: ignoring write data from periphal, ch {}: channel not set for write transfer ({})", ch_num, transfer);
        return;
    }
    if ((ch.mode & (mode::AutoInit | mode::Reverse)) != 0) {
        spdlog::error("dma: ignoring write data from periphal, ch {}: unsupported mode {:x}", ch_num, ch.mode);
        return;
    }

    auto address = ch.address;
    spdlog::info("dma: write data from periphal, ch {}, length {} to address {:x}", ch_num, data.size(), address);
    for(size_t n = 0; n < data.size(); ++n) {
        memory.WriteByte(address + n, data[n]);
    }

    status |= (1 << ch_num); // set transfer complete
    mask |= (1 << ch_num); // mask channel
}

void DMA::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("dma: out16({:x}, {:x})", port, val);
}

uint8_t DMA::Impl::In8(io_port port)
{
    spdlog::info("dma: in8({:x})", port);
    switch(port) {
        case io::Status_Read: {
            const auto v = status;
            status = status & 0xf0; // rest transfer-complete bits
            return v;
        }
    }
    return 0;
}

uint16_t DMA::Impl::In16(io_port port)
{
    spdlog::info("dma: in16({:x})", port);
    return 0;
}