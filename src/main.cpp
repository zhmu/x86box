#include "cpux86.h"
#include "hostio.h"
#include "io.h"
#include "memory.h"
#include "keyboard.h"
#include "vga.h"
#include "ata.h"
#include "pic.h"
#include "pit.h"
#include "dma.h"
#include "ppi.h"
#include "rtc.h"
#include "fdc.h"
#include "imagelibrary.h"

#include "disassembler.h"

#include <fstream>
#include <iostream>
#include <iomanip>

#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"

namespace {

template<typename Fn>
void load_rom(Memory& memory, const std::string& fname, Fn determineBaseAddr)
{
    std::ifstream ifs(fname, std::ifstream::binary);
    if (!ifs) throw std::runtime_error(std::string("cannot open '") + fname + "'");

    ifs.seekg(0, std::ifstream::end);
    auto length = ifs.tellg();
    ifs.seekg(0);

    const auto base = determineBaseAddr(length);
    const auto ptr = memory.GetPointer(base, length);
    if (!ptr) throw std::runtime_error("cannot obtain pointer to memory");
    spdlog::info("Loading BIOS at address 0x{:x}", base);

    ifs.read(static_cast<char*>(ptr), length);
    if (!ifs || ifs.gcount() != length)
        throw std::runtime_error("read error");
}

}

// Use SPDLOG_LEVEL=debug for debugging
int main(int argc, char** argv)
{
    argparse::ArgumentParser prog("x86box");
    prog.add_argument("--bios")
        .help("use specific bios image")
        .default_value(std::string("../../images/bios.bin"))
        .required();
    prog.add_argument("--rom")
        .help("use option rom");
    prog.add_argument("--fd0")
        .append()
        .help("use specified image for floppy disk 0");
    prog.add_argument("--hd0")
        .help("use specified image for hard disk 0");
    prog.add_argument("-d", "--disassemble")
        .help("enable live disassembly of code prior to execution")
        .default_value(false)
        .implicit_value(true);
    try {
        prog.parse_args(argc, argv);
    } catch(const std::runtime_error& e) {
        std::cerr << e.what() << '\n';
        std::cerr << prog;
        return 1;
    }

    spdlog::cfg::load_env_levels();

    auto imageLibrary = std::make_unique<ImageLibrary>();
    auto memory = std::make_unique<Memory>();
    auto io = std::make_unique<IO>();
    auto x86cpu = std::make_unique<CPUx86>(*memory, *io);
    auto hostio = std::make_unique<HostIO>();
    auto ata = std::make_unique<ATA>(*io, imageLibrary->GetImageProvider());
    auto pic = std::make_unique<PIC>(*io);
    auto pit = std::make_unique<PIT>(*io);
    auto dma = std::make_unique<DMA>(*io, *memory);
    auto ppi = std::make_unique<PPI>(*io, *pit);
    auto rtc = std::make_unique<RTC>(*io);
    auto fdc = std::make_unique<FDC>(*io, *pic, *dma, imageLibrary->GetImageProvider());
    auto vga = std::make_unique<VGA>(*memory, *io, *hostio);
    auto keyboard = std::make_unique<Keyboard>(*io, *hostio);
    std::unique_ptr<Disassembler> disassembler;
    if (prog["disassemble"] == true)
        disassembler = std::make_unique<Disassembler>();


    memory->Reset();
    io->Reset();
    x86cpu->Reset();
    vga->Reset();
    keyboard->Reset();
    ata->Reset();
    pic->Reset();
    pit->Reset();
    dma->Reset();
    rtc->Reset();
    fdc->Reset();

    load_rom(*memory, prog.get<std::string>("bios"), [](size_t length) { return 0x100000 - length; });
    if (auto rom = prog.present("--rom"); rom) {
        //"../../images/ide_xtl_padded.bin"
        load_rom(*memory, *rom, [](size_t length) { return 0xe8000; });
    }

    if (auto hd0 = prog.present("--hd0"); hd0 && !imageLibrary->SetImage(Image::Harddisk0, hd0->c_str())) {
        std::cerr << "Unable to attach hard disk image '" << *hd0 << "'\n";
        return -1;
    }

    size_t fd0image_current_index = 0;
    const auto fd0images = prog.get<std::vector<std::string>>("--fd0");
    if (!fd0images.empty() && !imageLibrary->SetImage(Image::Floppy0, fd0images.front().c_str())) {
        std::cerr << "Unable to attach hard disk image '" << fd0images.front() << "'\n";
        return -1;
    }

    int num_zero_instr = 0;
    unsigned int n = 0, m = 0;
    bool disassembler_active = false;

    bool running = true;
    while(running) {
        if (const auto event = hostio->GetPendingEvent(); event) {
            switch(*event) {
                case HostIO::EventType::Terminate:
                    running = false;
                    continue;
                case HostIO::EventType::ChangeImageFloppy0:
                    if (fd0images.size() > 1) {
                        fd0image_current_index = (fd0image_current_index + 1) % fd0images.size();
                        const auto& fd0image = fd0images[fd0image_current_index];
                        if (imageLibrary->SetImage(Image::Floppy0, fd0image.c_str())) {
                            spdlog::info("main: fd0 now uses image '{}'", fd0image);
                            fdc->NotifyImageChanged();
                        } else {
                            spdlog::error("main: unable to use image '{}' for fd0", fd0image);
                        }
                    }
                    break;
            }
        }

        if (x86cpu->GetState().m_flags & cpu::flag::IF) {
            auto irq = pic->DequeuePendingIRQ();
            if (irq) {
                spdlog::info("main: triggering irq {}", *irq);
                x86cpu->HandleInterrupt(*irq);
            }
        }

        if (x86cpu->GetState().m_cs == 0 && x86cpu->GetState().m_ip == 0x7c00)
            disassembler_active = true;
        if (disassembler && disassembler_active) {
            const auto s = disassembler->Disassemble(*memory, x86cpu->GetState());
            std::cout << s << '\n';
        }

        x86cpu->RunInstruction();
        if (disassembler_active) x86cpu->Dump();
        if (++n >= 500) {
            vga->Update();
            hostio->Update();
            n = 0;
        }

        if (pit->Tick()) {
            pic->AssertIRQ(0);
        }

        while (true) {
            const auto scancode = hostio->GetAndClearPendingScanCode();
            if (!scancode)
                break;
            keyboard->EnqueueScancode(scancode);
            pic->AssertIRQ(1);
        }
    }
    return 0;
}
