#include "dma.h"
#include "io.h"

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
        constexpr inline io_port ClearMask_Write = Base + 0xe;
        constexpr inline io_port WriteMask = Base + 0xf;
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

        constexpr inline uint8_t AutoInit = (1 << 5);

        constexpr inline uint8_t DemandMode = 0b00 << 6;
        constexpr inline uint8_t SingleMode = 0b01 << 6;
        constexpr inline uint8_t BlockMode = 0b10 << 6;
        constexpr inline uint8_t CascadeMode = 0b11 << 6;
        constexpr inline uint8_t ModeMask = 0b11 << 6;
    }
}

struct DMA::Impl : IOPeripheral
{
    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};

DMA::DMA(IO& io)
    : impl(std::make_unique<Impl>())
{
    io.AddPeripheral(io::Base, 10, *impl);
}

DMA::~DMA() = default;

void DMA::Reset()
{
}

void DMA::Impl::Out8(io_port port, uint8_t val)
{
    spdlog::info("dma: out8({:x}, {:x})", port, val);
}

void DMA::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("dma: out16({:x}, {:x})", port, val);
}

uint8_t DMA::Impl::In8(io_port port)
{
    spdlog::info("dma: in8({:x})", port);
    return 0;
}

uint16_t DMA::Impl::In16(io_port port)
{
    spdlog::info("dma: in16({:x})", port);
    return 0;
}
