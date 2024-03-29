#include "cpu/cpux86.h"
#include "platform/hostio.h"
#include "bus/io.h"
#include "bus/memory.h"
#include "hw/keyboard.h"
#include "hw/vga.h"
#include "hw/ata.h"
#include "hw/pic.h"
#include "hw/pit.h"
#include "hw/dma.h"
#include "hw/ppi.h"
#include "hw/rtc.h"
#include "hw/fdc.h"
#include "platform/imagelibrary.h"
#include "platform/tickprovider.h"
#include "platform/timeprovider.h"

#include "cpu/disassembler.h"

#include <fstream>
#include <csignal>
#include <iostream>
#include <iomanip>

#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace {

std::shared_ptr<spdlog::logger> trace_logger;
bool running = true;

constexpr inline auto emulatorCyclesPriorToUpdate = 500;

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

CPUx86::addr_t decode_address(const std::string& s)
{
    if (const auto n = s.find(':'); n != std::string::npos) {
        uint16_t cs{};
        if (const auto [ _, ec ] = std::from_chars(s.data(), s.data() + n, cs, 16); ec != std::errc()) {
            throw std::runtime_error(std::string("unable to parse integer from segment '") + s + "'");
        }
        uint16_t ip{};
        if (const auto [ _, ec ] = std::from_chars(s.data() + n + 1, s.data() + s.size(), ip, 16); ec != std::errc()) {
            throw std::runtime_error(std::string("unable to parse integer from offset '") + s + "'");
        }
        return CPUx86::MakeAddr(cs, ip);
    }

    CPUx86::addr_t result{};
    const auto [ _, ec ] = std::from_chars(s.data(), s.data() + s.size(), result, 16);
    if (ec != std::errc()) {
        throw std::runtime_error(std::string("unable to parse integer '") + s + "'");
    }
    return result;
}

void LogState(const cpu::State& st)
{
    trace_logger->info("ax={:04x} bx={:04x} cx={:04x} dx={:04x} si={:04x} di={:04x} bp={:04x} flags={:04x}", st.m_ax,
        st.m_bx, st.m_cx, st.m_dx, st.m_si, st.m_di, st.m_bp, st.m_flags);
    trace_logger->info("cs:ip={:04x}:{:04x} ds={:04x} es={:04x} ss:sp={:04x}:{:04x}", st.m_cs, st.m_ip, st.m_ds, st.m_es, st.m_ss,
        st.m_sp);
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
    prog.add_argument("--hd1")
        .help("use specified image for hard disk 1");
    prog.add_argument("--vgabios")
        .help("use specified bios image as VGA bios");
    prog.add_argument("-d", "--disassemble")
        .help("enable live disassembly of code prior to execution once specified address is executing");
    try {
        prog.parse_args(argc, argv);
    } catch(const std::runtime_error& e) {
        std::cerr << e.what() << '\n';
        std::cerr << prog;
        return 1;
    }

    spdlog::set_level(spdlog::level::warn);
    spdlog::cfg::load_env_levels();

    auto imageLibrary = std::make_unique<ImageLibrary>();
    auto tick = std::make_unique<TickProvider>();
    auto time = std::make_unique<TimeProvider>();
    auto memory = std::make_unique<Memory>();
    auto io = std::make_unique<IO>();
    auto x86cpu = std::make_unique<CPUx86>(*memory, *io);
    auto hostio = std::make_unique<HostIO>();
    auto ata = std::make_unique<ATA>(*io, imageLibrary->GetImageProvider());
    auto pic = std::make_unique<PIC>(*io);
    auto pit = std::make_unique<PIT>(*io, *tick);
    auto dma = std::make_unique<DMA>(*io, *memory);
    auto ppi = std::make_unique<PPI>(*io, *pit);
    auto rtc = std::make_unique<RTC>(*io, *time);
    auto fdc = std::make_unique<FDC>(*io, *pic, *dma, imageLibrary->GetImageProvider());
    auto vga = std::make_unique<VGA>(*memory, *io, *hostio, *tick);
    auto keyboard = std::make_unique<Keyboard>(*io, *hostio);

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
        load_rom(*memory, *rom, [](size_t length) { return 0xe8000; });
    }
    if (auto vgabios = prog.present("--vgabios"); vgabios) {
        load_rom(*memory, *vgabios, [](size_t) { return 0xc0000; });
    }

    if (auto hd0 = prog.present("--hd0"); hd0 && !imageLibrary->SetImage(Image::Harddisk0, hd0->c_str())) {
        std::cerr << "Unable to attach hard disk image '" << *hd0 << "'\n";
        return -1;
    }
    if (auto hd1 = prog.present("--hd1"); hd1 && !imageLibrary->SetImage(Image::Harddisk1, hd1->c_str())) {
        std::cerr << "Unable to attach hard disk image '" << *hd1 << "'\n";
        return -1;
    }

    size_t fd0image_current_index = 0;
    const auto fd0images = prog.get<std::vector<std::string>>("--fd0");
    if (!fd0images.empty() && !imageLibrary->SetImage(Image::Floppy0, fd0images.front().c_str())) {
        std::cerr << "Unable to attach floppy disk image '" << fd0images.front() << "'\n";
        return -1;
    }

    trace_logger = spdlog::stderr_color_st("trace");

    std::optional<CPUx86::addr_t> disassemble_address;
    if (auto disasm = prog.present("--disassemble"); disasm) {
        disassemble_address = decode_address(*disasm);
    }

    signal(SIGINT, [](int) { running = false; });

    std::unique_ptr<Disassembler> disassembler;
    unsigned int emulatorCycle = 0;
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

        if (cpu::FlagInterrupt(x86cpu->GetState().m_flags)) {
            if (const auto irq = pic->DequeuePendingIRQ(); irq) {
                x86cpu->HandleInterrupt(*irq);
            }
        }

        if (!disassembler && disassemble_address) {
            if (const auto csip = CPUx86::MakeAddr(x86cpu->GetState().m_cs, x86cpu->GetState().m_ip); csip == *disassemble_address) {
                disassembler = std::make_unique<Disassembler>();
            }
        }

        if (disassembler) {
            const auto s = disassembler->Disassemble(*memory, x86cpu->GetState());
            trace_logger->info(s);
        }

        x86cpu->RunInstruction();
        if (disassembler) {
            LogState(x86cpu->GetState());
        }

        if (vga->Update()) {
            hostio->Render();
        }

        if (++emulatorCycle >= emulatorCyclesPriorToUpdate) {
            hostio->Update();
            emulatorCycle = 0;
        }

        if (pit->Tick()) {
            pic->AssertIRQ(PIC::IRQ::PIT);
        }

        while (true) {
            const auto scancode = hostio->GetAndClearPendingScanCode();
            if (!scancode)
                break;
            keyboard->EnqueueScancode(scancode);
        }
        pic->SetPendingIRQState(PIC::IRQ::Keyboard, keyboard->IsQueueFilled());
    }

    printf("stopped at cs:ip=%04x:%04x\n", x86cpu->GetState().m_cs, x86cpu->GetState().m_ip);
    return 0;
}
