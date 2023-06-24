#include "ata.h"

#include "spdlog/spdlog.h"

namespace
{
    namespace io
    {
        static constexpr inline io_port Base = 0x300;

        static constexpr inline io_port Data = Base + 0x0;
        static constexpr inline io_port Error_Read = Base + 0x1;
        static constexpr inline io_port Feature_Write = Base + 0x1;
        static constexpr inline io_port SectorCount = Base + 0x2;
        static constexpr inline io_port SectorNumber = Base + 0x3;
        static constexpr inline io_port CylinderLow = Base + 0x4;
        static constexpr inline io_port CylinderHigh = Base + 0x5;
        static constexpr inline io_port DriveHead = Base + 0x6;

        // 3f6 for primary IDE
        static constexpr inline io_port AltStatus = Base + 0x7;
        static constexpr inline io_port DevControl = Base + 0x7;
    }

    namespace status
    {
        static constexpr inline uint8_t Error = (1 << 0);
        static constexpr inline uint8_t Index = (1 << 1);
        static constexpr inline uint8_t CorrectedData = (1 << 2);
        static constexpr inline uint8_t DataRequest = (1 << 3);
        static constexpr inline uint8_t ServiceRequest = (1 << 4);
        static constexpr inline uint8_t DriveFault = (1 << 5);
        static constexpr inline uint8_t Ready = (1 << 6);
        static constexpr inline uint8_t Busy = (1 << 7);
    }
}

ATA::ATA(IO& io)
{
    io.AddPeripheral(io::Base, 16, *this);
}

void ATA::Reset()
{
}

void ATA::Out8(io_port port, uint8_t val)
{
    spdlog::info("ata: out8({:x}, {:x})", port, val);
}

uint8_t ATA::In8(io_port port)
{
    spdlog::info("ata: in8({:x})", port);
    switch (port)
    {
        case io::AltStatus:
            return status::Ready;
    }
    return 0;
}

void ATA::Out16(io_port port, uint16_t val)
{
    spdlog::info("ata: out16({:x}, {:x})", port, val);
}

uint16_t ATA::In16(io_port port)
{
    spdlog::info("ata: in16({:x})", port);
    return 0;
}
