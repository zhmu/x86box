#include "cpux86.h"
#include "dos.h"
#include "hostio.h"
#include "io.h"
#include "memory.h"
#include "keyboard.h"
#include "vectors.h"
#include "vga.h"
#include <stdio.h>
#include <stdlib.h>

#include "spdlog/cfg/env.h"

Memory memory;
IO io;
Vectors vectors(memory);
CPUx86 x86cpu(memory, io, vectors);
HostIO hostio;
DOS dos(x86cpu, memory, vectors);
VGA vga(memory, hostio, vectors);
Keyboard keyboard(memory, hostio, vectors);

static bool load_to_memory(const char* fname, uint32_t base)
{
    FILE* f = fopen(fname, "rb");
    if (f == NULL)
        return false;

    while (true) {
        char buf[1024];
        unsigned int len = fread(buf, 1, sizeof(buf), f);
        if (len <= 0)
            break;
        for (unsigned int n = 0; n < len; n++)
            memory.WriteByte(base + n, buf[n]);
        base += len;
    }

    fclose(f);
    return true;
}

int main(int argc, char** argv)
{
    spdlog::cfg::load_env_levels();

    memory.Reset();
    memory.AddPeripheral(0xa0000, 65535, vga);
    memory.AddPeripheral(0xb0000, 65535, vga);
    io.Reset();
    dos.Reset();
    x86cpu.Reset();
    vga.Reset();
    keyboard.Reset();

    if (!load_to_memory("../images/snake.bin", 0x10100))
        abort();
    x86cpu.SetupForCOM(0x1000);

    if (0) {
        std::ifstream ifs("../images/maze.exe");
        const auto err = dos.LoadEXE(ifs);
        if (err != DOS::ErrorCode::Success) {
            fprintf(stderr, "can't load exe: %u\n", err);
            return 1;
        }
    }
    if (!hostio.Initialize())
        return 1;

#if 0
	for (int i = 0; i < 1000; i++) {
		x86cpu.RunInstruction();
		x86cpu.Dump();
  }
return 1;

#endif

    unsigned int n = 0, m = 0;
    while (!hostio.Quitting()) {
        x86cpu.RunInstruction();
        x86cpu.Dump();
        if (++n >= 500) {
            vga.Update();
            hostio.Update();
            n = 0;
        }

        if (++m > 10000) {
            memory.WriteWord(0x46c, memory.ReadWord(0x46c) + 1);
            m = 0;
        }
    }
    hostio.Cleanup();
    return 0;
}
