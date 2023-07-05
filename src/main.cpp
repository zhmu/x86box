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

    auto memory = std::make_unique<Memory>();
    auto io = std::make_unique<IO>();
    auto x86cpu = std::make_unique<CPUx86>(*memory, *io);
    auto hostio = std::make_unique<HostIO>();
    auto ata = std::make_unique<ATA>(*io);
    auto pic = std::make_unique<PIC>(*io);
    auto pit = std::make_unique<PIT>(*io);
    auto dma = std::make_unique<DMA>(*io, *memory);
    auto ppi = std::make_unique<PPI>(*io, *pit);
    auto rtc = std::make_unique<RTC>(*io);
    auto fdc = std::make_unique<FDC>(*io, *pic, *dma);
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

    unsigned int n = 0, m = 0;
    bool disassembler_active = false;
    while (!hostio->IsQuitting()) {
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

        if (const auto scancode = hostio->GetAndClearPendingScanCode()) {
            keyboard->EnqueueScancode(scancode);
            pic->AssertIRQ(1);
        }
    }
    return 0;
}
