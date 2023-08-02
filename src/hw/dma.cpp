#include "dma.h"
#include "../interface/iointerface.h"
#include "../interface/memoryinterface.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

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

    struct Transfer : DMATransfer
    {
        const int ch_num;
        DMA::Impl& impl;

        Transfer(int ch, DMA::Impl& impl) : ch_num(ch), impl(impl) { }
        size_t WriteFromPeripheral(uint16_t offset, std::span<const uint8_t> data) override;
        size_t GetTotalLength() override;
        void Complete() override;
    };
}

struct DMA::Impl : IOPeripheral
{
    MemoryInterface& memory;
    std::shared_ptr<spdlog::logger> logger;

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

    Impl(IOInterface& io, MemoryInterface& memory);

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;

    void Reset();
};

DMA::DMA(IOInterface& io, MemoryInterface& memory)
    : impl(std::make_unique<Impl>(io, memory))
{
}

DMA::~DMA() = default;

void DMA::Reset()
{
    impl->Reset();
}

std::unique_ptr<DMATransfer> DMA::InitiateTransfer(int ch_num)
{
    return std::make_unique<Transfer>(ch_num, *impl);
}

DMA::Impl::Impl(IOInterface& io, MemoryInterface& memory)
    : memory(memory)
    , logger(spdlog::stderr_color_st("dma"))
{
    io.AddPeripheral(io::Base, 16, *this);
    io.AddPeripheral(io::Ch0_PageAddr, 1, *this);
    io.AddPeripheral(io::Ch1_PageAddr, 1, *this);
    io.AddPeripheral(io::Ch2_PageAddr, 1, *this);
    io.AddPeripheral(io::Ch3_PageAddr, 1, *this);
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
    logger->info("out8({:x}, {:x})", port, val);
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

size_t Transfer::GetTotalLength()
{
    auto& ch = impl.channel[ch_num];
    return ch.count + 1;
}

size_t Transfer::WriteFromPeripheral(uint16_t offset, std::span<const uint8_t> data)
{
    impl.logger->info("ch{}: write data from periphal, offset {}, length {}", ch_num, offset, data.size());
    if (impl.mask & (1 << ch_num)) {
        impl.logger->error("ch{}: ignoring write data from periphal, channel is masked", ch_num);
        return 0;
    }

    auto& ch = impl.channel[ch_num];
    const auto transfer = ch.mode & mode::TransferMask;
    if (transfer != mode::WriteTransfer && transfer != mode::VerifyTransfer) {
        impl.logger->error("ch{}: ignoring write data from periphal: channel not set for write/verify transfer ({})", ch_num, transfer);
        return 0;
    }
    if ((ch.mode & (mode::AutoInit | mode::Reverse)) != 0) {
        impl.logger->error("ch{}: ignoring write data from periphal: unsupported mode {:x}", ch_num, ch.mode);
        return 0;
    }

    const auto total_length = GetTotalLength();
    if (offset + data.size() > total_length) {
        impl.logger->error("ch{}: ignoring write data from periphal: attempt to write beyond buffer (needed {}, have {})", ch_num, offset + data.size(), total_length);
        return 0;
    }

    if (transfer == mode::WriteTransfer) {
        const auto address = ch.address + offset;
        impl.logger->debug("ch{}: write data from periphal, length {} to address {:x}", ch_num, data.size(), address);
        for(size_t n = 0; n < data.size(); ++n) {
            impl.memory.WriteByte(address + n, data[n]);
        }
    }
    return data.size();
}

void Transfer::Complete()
{
    impl.status |= (1 << ch_num); // set transfer complete
    impl.mask |= (1 << ch_num); // mask channel
}

void DMA::Impl::Out16(io_port port, uint16_t val)
{
    logger->info("out16({:x}, {:x})", port, val);
}

uint8_t DMA::Impl::In8(io_port port)
{
    logger->info("in8({:x})", port);
    switch(port) {
        case io::Status_Read: {
            const auto v = status;
            status = status & 0xf0; // reset transfer-complete bits
            return v;
        }
    }
    return 0;
}

uint16_t DMA::Impl::In16(io_port port)
{
    logger->info("in16({:x})", port);
    return 0;
}
