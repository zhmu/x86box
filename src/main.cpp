#include "cpux86.h"
#include "hostio.h"
#include "io.h"
#include "memory.h"
#include "keyboard.h"
#include "vga.h"
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <iomanip>

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"

namespace {

Memory memory;
IO io;
CPUx86 x86cpu(memory, io);
HostIO hostio;
VGA vga(memory, io, hostio);
Keyboard keyboard(memory, io, hostio);

bool load_to_memory(const char* fname, uint32_t base)
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

void load_bios(const char* fname)
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

    memory.Reset();
    io.Reset();
    x86cpu.Reset();
    vga.Reset();
    keyboard.Reset();
    load_bios("../../images/bios.bin");
    if (!load_to_memory("../../images/ide_xtl_padded.bin", 0xe8000))
        abort();
    if (!hostio.Initialize())
        return 1;

    unsigned int n = 0, m = 0;
    while (!hostio.Quitting()) {
        x86cpu.RunInstruction();
        x86cpu.Dump();
        if (++n >= 500) {
            vga.Update();
            hostio.Update();
            n = 0;
        }
    }
    hostio.Cleanup();
    return 0;
}
