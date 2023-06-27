#include "fdc.h"
#include "io.h"
#include "pic.h"
#include "dma.h"

#include "spdlog/spdlog.h"

#include <unistd.h>
#include <fcntl.h>

// Intel 82077A
namespace
{
    constexpr inline uint8_t IRQ_FDC = 6;

    namespace io
    {
        constexpr inline io_port Base = 0x3f0;
        constexpr inline io_port StatusA = Base + 0x0;
        constexpr inline io_port StatusB = Base + 0x1;
        constexpr inline io_port DigitalOutput = Base + 0x2;
        constexpr inline io_port TapeDrive = Base + 0x3;
        constexpr inline io_port MainStatus_Read = Base + 0x4;
        constexpr inline io_port DataRate_Write = Base + 0x4;
        constexpr inline io_port DataFifo = Base + 0x5;
        constexpr inline io_port DigitalInput_Read = Base + 0x7;
        constexpr inline io_port ConfigControl_Write  = Base + 0x7;
    }

    namespace dor
    {
        constexpr inline uint8_t Motor3Enable = (0b1 << 7);
        constexpr inline uint8_t Motor2Enable = (0b1 << 6);
        constexpr inline uint8_t Motor1Enable = (0b1 << 5);
        constexpr inline uint8_t Motor0Enable = (0b1 << 4);
        constexpr inline uint8_t DMAGateN = (0b1 << 3);
        constexpr inline uint8_t ResetN = (0b1 << 2);
        constexpr inline uint8_t Drive1Select = (0b1 << 1);
        constexpr inline uint8_t Drive0Select = (0b1 << 0);
    }

    namespace dsr
    {
        constexpr inline uint8_t SWReset = (0b1 << 7);
        constexpr inline uint8_t PowerDown = (0b1 << 6);
        constexpr inline uint8_t Precomp2 = (0b1 << 4);
        constexpr inline uint8_t Precomp1 = (0b1 << 3);
        constexpr inline uint8_t Precomp0 = (0b1 << 2);
        constexpr inline uint8_t DataRateSelect1 = (0b1 << 1);
        constexpr inline uint8_t DataRateSelect0 = (0b1 << 0);
    }

    namespace msr
    {
        constexpr inline uint8_t HostTransferData = (0b1 << 7); // RQM
        constexpr inline uint8_t TransferDirection = (0b1 << 6); // DIO
        constexpr inline uint8_t NonDMA = (0b1 << 5);
        constexpr inline uint8_t CommandBusy = (0b1 << 4); // CMD-BSY
        constexpr inline uint8_t Drive3Busy = (0b1 << 3); // DRV 3 BUSY
        constexpr inline uint8_t Drive2Busy = (0b1 << 2); // DRV 2 BUSY
        constexpr inline uint8_t Drive1Busy = (0b1 << 1); // DRV 1 BUSY
        constexpr inline uint8_t Drive0Busy = (0b1 << 0); // DRV 0 BUSY
    }

    // 6.1 Status Register 0
    namespace st0
    {
        constexpr inline uint8_t InterruptCode1 = (0b1 << 7); // IC
        constexpr inline uint8_t InterruptCode0 = (0b1 << 6);
        constexpr inline uint8_t SeekEnd = (0b1 << 5); // SE
        constexpr inline uint8_t EquipmentCheck = (0b1 << 4); // EC
        constexpr inline uint8_t HeadAddress = (0b1 << 2); // H
        constexpr inline uint8_t DriveSelect1 = (0b1 << 1); // DS1
        constexpr inline uint8_t DriveSelect0 = (0b1 << 0); // DS0
    }

    // 6.2 Status Register 1
    namespace st1
    {
        constexpr inline uint8_t EndOfCylinder = (0b1 << 7); // EC
        constexpr inline uint8_t DataError = (0b1 << 5); // DE
        constexpr inline uint8_t Overrun = (0b1 << 4); // OR
        constexpr inline uint8_t NoData = (0b1 << 2); // ND
        constexpr inline uint8_t NotWritable  = (0b1 << 1); // NW
        constexpr inline uint8_t MissingAddressMark = (0b1 << 0); // MA
    }

    // 6.3 Status Register 2
    namespace st2
    {
        constexpr inline uint8_t ControlMark = (0b1 << 6); // CM
        constexpr inline uint8_t DataError = (0b1 << 5); // DD
        constexpr inline uint8_t WrongCylinder = (0b1 << 4); // WC
        constexpr inline uint8_t BadCylinder = (0b1 << 1); // BC
        constexpr inline uint8_t MissingDataAddressMark = (0b1 << 0); // MD
    }

    // 6.4 Status Register 3
    namespace st3
    {
        constexpr inline uint8_t WriteProtected = (0b1 << 6); // WP
        constexpr inline uint8_t Track0 = (0b1 << 4); // T0
        constexpr inline uint8_t HeadAddress = (0b1 << 2); // HD
        constexpr inline uint8_t DriveSelect1 = (0b1 << 1); // DS1
        constexpr inline uint8_t DriveSelect0 = (0b1 << 0); // DS0
    }

    // Table 5-1
    namespace command {
        constexpr inline uint8_t MultiTrack = (0b1 << 7); // MT
        constexpr inline uint8_t MFMEncoding = (0b1 << 6); // MFM
        constexpr inline uint8_t SkipMode = (0b1 << 5); // SK

        constexpr inline uint8_t Specify = 3;
        constexpr inline uint8_t WriteData = 5;
        constexpr inline uint8_t ReadData = 6;
        constexpr inline uint8_t Recalibrate = 7;
        constexpr inline uint8_t SenseInterruptStatus = 8; // 5.2.4
        // Should yield ST0 (Status Register 0) and PCN (Present Cylinder Number)
        constexpr inline uint8_t ReadID = 10;
        constexpr inline uint8_t FormatTrack = 13;
        constexpr inline uint8_t Seek = 15;
        constexpr inline uint8_t Configure = 19;
    }

    size_t DetermineNumberOfInputBytes(uint8_t cmd)
    {
        switch(cmd & 0b00011111)
        {
            case command::WriteData:
            case command::ReadData:
                return 9;
            case command::Recalibrate:
            case command::ReadID:
                return 2;
            case command::SenseInterruptStatus:
                return 1;
            case command::FormatTrack:
                return 6;
            case command::Seek:
            case command::Specify:
                return 3;
            case command::Configure:
                return 7;
        }
        return 1; // just the command byte
    }
}

struct FDC::Impl : IOPeripheral
{
    PIC& pic;
    DMA& dma;
    uint8_t dor = 0x0;
    std::array<uint8_t, 16> fifo{};
    size_t fifoWriteOffset = 0;
    size_t fifoReadOffset = 0;
    size_t fifoReadBytesAvailable = 0;
    uint8_t st0 = 0;
    uint8_t current_track = 0;

    enum class State {
        Idle,
        ReceiveCommandBytes,
        TransmitFifoBytes,
    } state = State::Idle;

    Impl(PIC& pic, DMA& dma) : pic(pic), dma(dma) { }

    void Reset();
    bool ExecuteCurrentCommand();

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};

FDC::FDC(IO& io, PIC& pic, DMA& dma)
    : impl(std::make_unique<Impl>(pic, dma))
{
    io.AddPeripheral(io::Base, 8, *impl);
}

FDC::~FDC() = default;

void FDC::Reset()
{
    impl->Reset();
}

void FDC::Impl::Reset()
{
    dor = 0x0;
    state = Impl::State::Idle;
    fifoReadOffset = 0;
    fifoWriteOffset = 0;
    fifoReadBytesAvailable = 0;
    st0 = st0::InterruptCode1 | st0::InterruptCode0;
    current_track = 0;
}

void FDC::Impl::Out8(io_port port, uint8_t val)
{
    spdlog::info("fdc: out8({:x}, {:x})", port, val);
    switch(port)
    {
        case io::DigitalOutput: {
            if ((dor & dor::ResetN) == 0 && (val & dor::ResetN) != 0) {
                // Reset toggled from lo -> hi
                spdlog::warn("fdc: reset");
                Reset();
                pic.AssertIRQ(IRQ_FDC);
            }
            dor = val;
            break;
        }
        case io::DataFifo: {
            switch(state)
            {
                case State::Idle:
                    fifoWriteOffset = 0;
                    state = State::ReceiveCommandBytes;
                    [[fallthrough]];
                case State::ReceiveCommandBytes:
                    fifo[fifoWriteOffset] = val;
                    fifoWriteOffset += 1;
                    if (DetermineNumberOfInputBytes(fifo[0]) == fifoWriteOffset) {
                        spdlog::info("fdc: executing command {} (fifo contains {} bytes)", fifo[0], fifoWriteOffset);
                        if (ExecuteCurrentCommand()) {
                            spdlog::info("fdc: triggering interrupt upon command completion");
                            pic.AssertIRQ(IRQ_FDC);
                        }
                        if (fifoReadBytesAvailable > 0) {
                            state = State::TransmitFifoBytes;
                        } else {
                            state = State::Idle;
                        }
                    }
                    break;
                default:
                    spdlog::error("fdc: ignoring fifo write in state {}", static_cast<int>(state));
                    break;
            }
            break;
        }
    }
}

bool FDC::Impl::ExecuteCurrentCommand()
{
    fifoReadOffset = 0;
    fifoReadBytesAvailable = 0;
    auto storeByteInFifo = [&](const uint8_t value) {
        fifo[fifoReadBytesAvailable] = value;
        fifoReadBytesAvailable += 1;
    };

    const auto cmd = fifo[0];
    switch(cmd) {
        case command::SenseInterruptStatus: {
            uint8_t pcn = 0;
            storeByteInFifo(st0);
            storeByteInFifo(pcn);
            spdlog::info("fdc: command: sense interrupt status -> st0 {:x} pcn {:x}", st0, pcn);
            return false;
        }
        case command::Specify: {
            const auto srt = (fifo[1] & 0xf) >> 4;
            const auto hut = (fifo[1] & 0xf) & 0xf;
            const auto hlt = fifo[2] >> 1;
            const auto nd = (fifo[2] & 1) != 0;
            spdlog::info("fdc: command: specify, srt {:x} hut {:x} hlt {:x} nd {}", srt, hut, hlt, nd);
            return false;
        }
        case command::Recalibrate: {
            const auto ds = fifo[1];
            spdlog::info("fdc: command: recalibrate, {}", ds);
            current_track = 0;
            st0 = st0::SeekEnd /* | st0::EquipmentCheck */;
            return true;
        }
        case command::Seek: {
            const auto hds = (fifo[1] & 0x4) != 0;
            const auto ds1 = (fifo[1] & 0x2) != 0;
            const auto ds0 = (fifo[1] & 0x1) != 0;
            const auto ncn = fifo[2];
            spdlog::info("fdc: command: seek -> hds {:x} ds1 {} ds0 {} ncn {:x}", hds, ds1, ds0, ncn);
            return true;
        }
    }

    if ((cmd & 0b001111) == command::ReadID) {
        uint8_t st1 = 0, st2 = 0, c = 0, h = 0, r = 0, n = 2;
        storeByteInFifo(st0);
        storeByteInFifo(st1);
        storeByteInFifo(st2);
        storeByteInFifo(c); // cyl address (0..255)
        storeByteInFifo(h);  // head address (0/1)
        storeByteInFifo(r); // sector address
        storeByteInFifo(n); // sector size code (2 = 512 bytes)
        spdlog::info("fdc: command: read id -> st0 {:x} st1 {:x} st2 {:x} c {:x} h {:x} r {:x} n {:x}", st0, st1, st2, c, h, r, n);
        return true;
    }
    if ((cmd & 0b11111) == command::ReadData) {
        const auto mt = (fifo[0] & 0x80) != 0;
        const auto mfm = (fifo[0] & 0x40) != 0;
        const auto sk = (fifo[0] & 0x20) != 0;
        const auto hds = (fifo[1] & 0x4) != 0;
        const auto ds1 = (fifo[1] & 0x2) != 0;
        const auto ds0 = (fifo[1] & 0x1) != 0;
        const auto c = fifo[2]; // cylinder
        const auto h = fifo[3]; // head
        const auto r = fifo[4]; // sector number
        const auto n = fifo[5]; // sector size codde (must be 2)
        const auto eot = fifo[6]; // end-of-track (final sector number)
        const auto gpl = fifo[7]; // gap length
        const auto dtl = fifo[8]; // special sector size (if n==0)
        spdlog::info("fdc: command: read data -> mt {} mfm {} sk {} hds {} ds1 {} ds0 {} c {} h {} r {} n {} eot {} gpl {} dtl {}", mt, mfm, sk, hds, ds1, ds0, c, h, r, n, eot, gpl, dtl);

        // Tell DMA to transfer one sector
        std::array<uint8_t, 512> sector;
#if 0
        std::fill(sector.begin(), sector.end(), 0x0f);
        sector[510] = 0x55;
        sector[511] = 0xaa;
#else
        static int fd = -1;
        if (fd < 0) {
            fd = open("../../external/freedos/144m/x86BOOT.img", O_RDONLY);
            if (fd <0) std::abort();
        }
        constexpr auto NUM_HEADS = 2;
        constexpr auto NUM_SPT = 18;
        size_t offset = (c * NUM_HEADS + h) * NUM_SPT + (r - 1);
        offset *= 512;
        spdlog::warn("fdc: reading c {} h {} s {} from offset {}", c, h, r, offset);
        lseek(fd, offset, SEEK_SET);
        if (read(fd, sector.data(), 512) != 512) {
            std::abort();
        }
#endif
        dma.WriteDataFromPeriphal(2, sector);

        const uint8_t st1 = 0;
        const uint8_t st2 = 0;
        storeByteInFifo(st0);
        storeByteInFifo(st1);
        storeByteInFifo(st2);
        storeByteInFifo(c);
        storeByteInFifo(h);
        storeByteInFifo(r);
        storeByteInFifo(n);
        return true;
    }
    spdlog::warn("fdc: command: unimplemented command {:x}", fifo[0]);
    return false;
}

void FDC::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("fdc: out16({:x}, {:x})", port, val);
}

uint8_t FDC::Impl::In8(io_port port)
{
    spdlog::info("fdc: in8({:x})", port);
    switch(port) {
        case io::MainStatus_Read: {
            uint8_t msr = 0;
            switch(state) {
                case State::Idle:
                    msr |= msr::HostTransferData;
                    break;
                case State::ReceiveCommandBytes:
                    msr |= msr::HostTransferData;
                    break;
                case State::TransmitFifoBytes:
                    msr |= msr::HostTransferData;
                    msr |= msr::TransferDirection;
                    msr |= msr::CommandBusy;
                    break;
                default:
                    spdlog::warn("fdc: read msr in unexpected state {}", static_cast<int>(state));
                    break;
            }
            spdlog::warn("fdc: read msr in state {} -> {:x}", static_cast<int>(state), msr);
            return msr;
        }
        case io::DataFifo: {
            uint8_t result = 0;
            switch(state) {
                case State::TransmitFifoBytes:
                    if (fifoReadOffset < fifoReadBytesAvailable) {
                        result = fifo[fifoReadOffset];
                        fifoReadOffset += 1;
                        if (fifoReadOffset == fifoReadBytesAvailable) {
                            state = State::Idle;
                        }
                    } else {
                        spdlog::error("fdc: reading fifo beyond available bytes ({} >= {})", fifoReadOffset, fifoReadBytesAvailable);
                    }
                    break;
            }
            return result;
        }
    }
    return 0;
}

uint16_t FDC::Impl::In16(io_port port)
{
    spdlog::info("fdc: in16({:x})", port);
    return 0;
}
