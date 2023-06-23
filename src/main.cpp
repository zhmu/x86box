#include "cpux86.h"
#include "hostio.h"
#include "io.h"
#include "memory.h"
#include "keyboard.h"
#include "vga.h"
#include "ata.h"
#include "pic.h"
#include "pit.h"
#include <stdio.h>
#include <stdlib.h>

#include "disassembler.h"

#include <fstream>
#include <iostream>
#include <iomanip>

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"

namespace {


bool load_to_memory(Memory& memory, const char* fname, uint32_t base)
{
    FILE* f = fopen(fname, "rb");
    if (f == NULL)
        return false;

    while (true) {
        char buf[1024];
        unsigned int len = fread(buf, 1, sizeof(buf), f);
        if (len <= 0)
            break;
        for (unsigned int n = 0; n < len; n++) {
            memory.WriteByte(base + n, buf[n]);
        }
        base += len;
    }

    fclose(f);
    return true;
}

void load_bios(Memory& memory, const char* fname)
{
    std::ifstream ifs(fname, std::ifstream::binary);
    if (!ifs) throw std::runtime_error(std::string("cannot open '") + fname + "'");

    ifs.seekg(0, std::ifstream::end);
    auto length = ifs.tellg();
    ifs.seekg(0);

    const auto base = 0x100000 - length;
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
    spdlog::cfg::load_env_levels();

    auto memory = std::make_unique<Memory>();
    auto io = std::make_unique<IO>();
    auto x86cpu = std::make_unique<CPUx86>(*memory, *io);
    auto hostio = std::make_unique<HostIO>();
    auto ata = std::make_unique<ATA>(*io);
    auto pic = std::make_unique<PIC>(*io);
    auto pit = std::make_unique<PIT>(*io);
    auto vga = std::make_unique<VGA>(*memory, *io, *hostio);
    auto keyboard = std::make_unique<Keyboard>(*memory, *io, *hostio);
    std::unique_ptr<Disassembler> disassembler;
    if (true)
        disassembler = std::make_unique<Disassembler>();

    memory->Reset();
    io->Reset();
    x86cpu->Reset();
    vga->Reset();
    keyboard->Reset();
    ata->Reset();
    pic->Reset();
    pit->Reset();
    load_bios(*memory, "../../images/bios.bin");
    if (!load_to_memory(*memory, "../../images/ide_xtl_padded.bin", 0xe8000))
        abort();

    unsigned int n = 0, m = 0;
    while (!hostio->IsQuitting()) {
        if (x86cpu->GetState().m_flags & cpu::flag::IF) {
            auto irq = pic->DequeuePendingIRQ();
            if (irq) {
                spdlog::info("main: triggering irq {}", *irq);
                x86cpu->HandleInterrupt(*irq);
            }
        }

        if (disassembler) {
            const auto s = disassembler->Disassemble(*memory, x86cpu->GetState());
            std::cout << s << '\n';
        }

        x86cpu->RunInstruction();
        x86cpu->Dump();
        if (++n >= 500) {
            vga->Update();
            hostio->Update();
            n = 0;
        }

        if (pit->Tick()) {
            pic->AssertIRQ(0);
        }
    }
    return 0;
}
