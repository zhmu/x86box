#include "ata.h"
#include "../interface/imageprovider.h"
#include "../interface/iointerface.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <fcntl.h>
#include <unistd.h>

namespace
{
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

    namespace error
    {
        static constexpr inline uint8_t AddressMarkNotFound = (1 << 0);
        static constexpr inline uint8_t Track0NotFound = (1 << 1);
        static constexpr inline uint8_t MediaChangeRequested = (1 << 2);
        static constexpr inline uint8_t AbortedCommand = (1 << 3);
        static constexpr inline uint8_t IDNotFound = (1 << 4);
        static constexpr inline uint8_t MediaChanged = (1 << 5);
        static constexpr inline uint8_t UnrecorrectableDataError = (1 << 6);
    }

    namespace command
    {
        static constexpr inline uint8_t ReadSectors = 0x20;
        static constexpr inline uint8_t WriteSectors = 0x30;
        static constexpr inline uint8_t ReadSectorsWithVerify = 0x40;
        static constexpr inline uint8_t SetMultipleMode = 0xc6;
        static constexpr inline uint8_t Identify = 0xec;
        static constexpr inline uint8_t SetFeatures = 0xef;
    }

    struct Identify
    {
        /* 00 */ uint16_t general_config;
        /* 01 */ uint16_t num_cylinders;
        /* 02 */ uint16_t reserved2;
        /* 03 */ uint16_t num_heads;
        /* 04 */ uint16_t obsolete4;
        /* 05 */ uint16_t obsolete5;
        /* 06 */ uint16_t sectors_per_track;
        /* 07 */ uint16_t vendor789[3];
        /* 08 */ uint16_t serial[10];
        /* 20 */ uint16_t vendor20[2];
        /* 22 */ uint16_t read_write_long_vendor_bytes;
        /* 23 */ uint16_t firmware[4];
        /* 27 */ uint16_t model[20];
        /* 47 */ uint16_t vendor47;
        /* 48 */ uint16_t reserved48;
        /* 49 */ uint16_t capabilities;
        /* 50 */ uint16_t reserved50;
        /* 51 */ uint16_t pio_data_transfer_timing_mode;
        /* 52 */ uint16_t dma_data_transfer_timing_mode;
        /* 53 */ uint16_t valid_54to58_64to70;
        /* 54 */ uint16_t current_num_cylinders;
        /* 55 */ uint16_t current_num_heads;
        /* 56 */ uint16_t current_sectors_per_track;
        /* 57 */ uint16_t current_capacity[2];
        /* 59 */ uint16_t max_sectors_per_transfer;
        /* 60 */ uint16_t total_user_addressable_sectors[2];
        /* 62 */ uint16_t single_word_dma;
        /* 63 */ uint16_t multi_word_dma;
        /* 64 */ uint16_t advanced_pio_modes;
        /* 65 */ uint16_t min_multiword_dma_transfer_cycle_time;
        /* 66 */ uint16_t recomended_multiword_dma_transfer_cycle_time;
        /* 67 */ uint16_t min_pio_transfer_cycle_time1;
        /* 68 */ uint16_t min_pio_transfer_cycle_time2;
        /* 69 */ uint16_t reserved69[2];
        /* 71 */ uint16_t reserved71[57];
        /* 128 */ uint16_t vendor128[32];
        /* 160 */ uint16_t reserved160[96];
    } __attribute__((packed));
    static_assert(sizeof(Identify) == 512);

    // Emulate type 3 (30.6MB) - https://vintage-pc.tripod.com/types.html
    constexpr inline int NumCylinders = 615;
    constexpr inline int NumHeads = 6;
    constexpr inline int SectorsPerTrack = 17;

    uint32_t CHStoLBA(unsigned int cyl, unsigned int head, unsigned int sector)
    {
        // https://en.wikipedia.org/wiki/Logical_block_addressing
        return (cyl * NumHeads + head) * SectorsPerTrack + (sector - 1);
    }

    std::optional<Image> SelectedDeviceToImage(ImageProvider& imageProvider, int selected_device)
    {
        const auto image = (selected_device == 0) ? Image::Harddisk0 : Image::Harddisk1;
        if (imageProvider.GetSize(image) == 0) return {};
        return image;
    }
}

struct ATA::Impl : IOPeripheral
{
    ImageProvider& imageProvider;
    std::shared_ptr<spdlog::logger> logger;

    Impl(ImageProvider& imageProvider);
    ~Impl();
    void Reset();

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;

    void ExecuteCommand(uint8_t cmd);

    size_t selected_device{};
    uint8_t sector_count{};
    uint8_t sector_nr{};
    uint16_t cylinder{};
    uint8_t feature{};
    uint8_t head{};
    uint8_t error{};

    std::array<uint8_t, 512> sector_data{};
    size_t sector_data_offset = sector_data.size();

    enum class TransferMode {
        Idle,
        PeripheralToHost,
        HostToPeripheral
    } transferMode{TransferMode::Idle};
    uint64_t current_lba{};
    size_t sectors_left{};
};

ATA::ATA(IOInterface& io, ImageProvider& imageProvider)
    : impl(std::make_unique<Impl>(imageProvider))
{
    io.AddPeripheral(io::Base, 16, *impl);
}

ATA::~ATA() = default;

void ATA::Reset()
{
    impl->Reset();
}

ATA::Impl::Impl(ImageProvider& imageProvider)
    : imageProvider(imageProvider)
    , logger(spdlog::stderr_color_st("ata"))
{
    // dd if=/dev/zero of=/tmp/hdd.img bs=512 count=62730
}

ATA::Impl::~Impl()
{
    spdlog::drop("ata");
}

void ATA::Impl::Reset()
{
    selected_device = 0;
    sector_count = 0;
    sector_nr = 0;
    cylinder = 0;
    feature = 0;
    head = 0;
    error = 0;
    sector_data_offset = sector_data.size();

    transferMode = TransferMode::Idle;
    current_lba = 0;
    sectors_left = 0;
}

void ATA::Impl::Out8(io_port port, uint8_t val)
{
    logger->info("out8({:x}, {:x})", port, val);
    switch (port) {
        case io::Data:
            if (transferMode == TransferMode::HostToPeripheral && sector_data_offset < sector_data.size()) {
                sector_data[sector_data_offset] = val;
                ++sector_data_offset;

                if (sector_data_offset == sector_data.size()) {
                    const auto image = SelectedDeviceToImage(imageProvider, selected_device);
                    if (!image || imageProvider.Write(*image, current_lba * sector_data.size(), sector_data) != sector_data.size()) {
                        logger->critical("write error!!!");
                    }

                    --sectors_left;
                    if (sectors_left > 0) {
                        ++current_lba;
                        sector_data_offset = 0;
                    } else {
                        transferMode = TransferMode::Idle;
                    }
                }
            } else {
                logger->error("out8: Data, but no data needed");
            }
            break;
        case io::Feature_Write:
            feature = val;
            break;
        case io::SectorCount:
            sector_count = val;
            break;
        case io::SectorNumber:
            sector_nr = val;
            break;
        case io::CylinderLow:
            cylinder = (cylinder & 0xff00) | val;
            break;
        case io::CylinderHigh:
            cylinder = (cylinder & 0xff) | (val << 8);
            break;
        case io::DriveHead:
            selected_device = (val & 0x10) ? 1 : 0;
            head = val & 0xf;
            break;
        case io::DevControl:
            ExecuteCommand(val);
            break;
        default:
            logger->info("out8: unknown {:x} = {:x}", port, val);
            break;
    }
}

uint8_t ATA::Impl::In8(io_port port)
{
    logger->info("in8({:x})", port);
    switch (port)
    {
        case io::Data: {
            if (transferMode == TransferMode::PeripheralToHost && sector_data_offset < sector_data.size()) {
                const auto data = sector_data[sector_data_offset];
                ++sector_data_offset;
                logger->debug("in8: Data ({:x})", data);

                if (sector_data_offset == sector_data.size()) {
                    logger->warn("in8: Sector completed");

                    --sectors_left;
                    if (sectors_left > 0) {
                        ++current_lba;
                        const auto image = SelectedDeviceToImage(imageProvider, selected_device);
                        if (!image || imageProvider.Read(*image, current_lba * sector_data.size(), sector_data) != sector_data.size()) {
                            logger->critical("read error!!!");
                            std::fill(sector_data.begin(), sector_data.end(), 0xff);
                        }
                        sector_data_offset = 0;
                    } else {
                        transferMode = TransferMode::Idle;
                    }
                }

                return data;
            } else {
                logger->error("in8: Data, but no data available");
            }
            break;
        }
        case io::Error_Read:
            logger->info("in8: Error");
            break;
        case io::SectorCount:
            logger->info("in8: SectorCount");
            break;
        case io::SectorNumber:
            logger->info("in8: SectorNumber");
            break;
        case io::CylinderLow:
            logger->info("in8: CylinderLow");
            break;
        case io::CylinderHigh:
            logger->info("in8: CylinderHigh");
            break;
        case io::DriveHead:
            logger->info("in8: Drivehead");
            break;
        case io::AltStatus: {
            uint8_t status = 0;
            if (SelectedDeviceToImage(imageProvider, selected_device))
                status |= status::Ready;
            if (sector_data_offset < sector_data.size())
                status |= status::DataRequest;
            if (error != 0)
                status |= status::Error;
            logger->info("in8: AltStatus {:x}", status);
            return status;
        }
    }
    return 0;
}

void ATA::Impl::Out16(io_port port, uint16_t val)
{
    logger->info("out16({:x}, {:x})", port, val);
}

uint16_t ATA::Impl::In16(io_port port)
{
    logger->info("in16({:x})", port);
    return 0;
}

void ATA::Impl::ExecuteCommand(uint8_t cmd)
{
    switch(cmd) {
        case command::ReadSectors: {
            logger->info("command: Read Sectors ({:x}), device {}, sector_count {} cylinder {} head {} sector_nr {} feature {}",
                cmd, selected_device, sector_count, cylinder, head, sector_nr, feature);

            current_lba = CHStoLBA(cylinder, head, sector_nr);

            logger->info("read from c/h/s {}/{}/{} -> lba {}", cylinder, head, sector_nr, current_lba);
            const auto image = SelectedDeviceToImage(imageProvider, selected_device);
            if (!image || imageProvider.Read(*image, current_lba * sector_data.size(), sector_data) != sector_data.size()) {
                logger->critical("read error!!!");
                std::fill(sector_data.begin(), sector_data.end(), 0xff);
            }
            sector_data_offset = 0;
            sectors_left = sector_count;
            transferMode = TransferMode::PeripheralToHost;
            error = 0;
            break;
        }
        case command::ReadSectorsWithVerify: {
            logger->info("command: Read Sectors With Verify ({:x}), device {}, sector_count {} cylinder {} head {} sector_nr {} feature {}",
                cmd, selected_device, sector_count, cylinder, head, sector_nr, feature);
            error = 0;
            break;
        }
        case command::WriteSectors: {
            logger->info("command: Write Sectors ({:x}), device {}, sector_count {} cylinder {} head {} sector_nr {} feature {}",
                cmd, selected_device, sector_count, cylinder, head, sector_nr, feature);

            logger->info("write to c/h/s {}/{}/{} -> lba {}", cylinder, head, sector_nr, current_lba);
            current_lba = CHStoLBA(cylinder, head, sector_nr);

            sector_data_offset = 0;
            sectors_left = sector_count;
            transferMode = TransferMode::HostToPeripheral;
            error = 0;
            break;
        }
        case command::Identify: {
            logger->info("command: Identify ({:x}), device {}, sector_count {} cylinder {} head {} sector_nr {} feature {}",
                cmd, selected_device, sector_count, cylinder, head, sector_nr, feature);
            const auto image = SelectedDeviceToImage(imageProvider, selected_device);
            if (image) {
                Identify id{};
                id.general_config = (1 << 15);
                id.num_cylinders = NumCylinders;
                id.num_heads = NumHeads;
                id.sectors_per_track = SectorsPerTrack;

                auto write_string = [&](uint16_t* dest, size_t num_words, const char* s) {
                    for(size_t n = 0; n < num_words; ++n)
                        dest[n] = 0x2020;
                    for(size_t n = 0; n < strlen(s); ++n) {
                        auto& v = dest[n / 2];
                        if (n % 2)
                            v = (v & 0xff00) | s[n];
                        else
                            v = (v & 0xff) | (s[n] << 8);
                    }
                };

                write_string(&id.model[0], 20, "DUMMY DRIVE");

                memcpy(sector_data.data(), reinterpret_cast<const void*>(&id), sector_data.size());
                sector_data_offset = 0;
                sectors_left = 1;
                transferMode = TransferMode::PeripheralToHost;
                error = 0;
            } else {
                error = error::AbortedCommand;
            }
            break;
        }
        case command::SetMultipleMode: {
            logger->info("command: Set Multiple Mode ({:x}), device {}, sector_count {} cylinder {} head {} sector_nr {} feature {}",
                cmd, selected_device, sector_count, cylinder, head, sector_nr, feature);
            if (sector_count == 0 || sector_count == 1)  {
                error = 0;
            } else {
                error = error::AbortedCommand;
            }
            break;
        }
        case command::SetFeatures: {
            logger->info("command: Set Features ({:x}), device {}, sector_count {} cylinder {} head {} sector_nr {} feature {}",
                cmd, selected_device, sector_count, cylinder, head, sector_nr, feature);
            break;
        }
        default:
            logger->warn("unsupported command {:x}, device {}, sector_count {} cylinder {} head {} sector_nr {} feature {}",
                cmd, selected_device, sector_count, cylinder, head, sector_nr, feature);
            error = error::AbortedCommand;
            break;
    }
}
