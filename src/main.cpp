#include "cpux86.h"
#include "dos.h"
#include "hostio.h"
#include "file.h"
#include "io.h"
#include "memory.h"
#include "vectors.h"
#include "vga.h"
#include <stdio.h>
#include <stdlib.h>

Memory memory;
IO io;
Vectors vectors(memory);
CPUx86 cpu(memory, io, vectors);
HostIO hostio;
DOS dos(cpu, memory, vectors);
VGA vga(memory, hostio, vectors);

static bool
load_to_memory(const char* fname, uint32_t base)
{
	FILE* f = fopen(fname, "rb");
	if (f == NULL)
		return false;

	while(true) {
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

int
main(int argc, char** argv)
{
	memory.Reset();
	memory.AddPeripheral(0xa0000, 65535, vga);
	memory.AddPeripheral(0xb0000, 65535, vga);
	io.Reset();
	dos.Reset();
	cpu.Reset();
	vga.Reset();

	if (!load_to_memory("../images/snake.bin", 0x10100))
		abort();
	cpu.SetupForCOM(0x1000);

	if (0)
	{
		File oFile;
		if (!oFile.Open("../images/maze.exe", File::FLAG_READ))
			abort();
		DOS::ErrorCode err = dos.LoadEXE(oFile);
		if (err != DOS::E_SUCCESS) {
			fprintf(stderr, "can't load exe: %u\n", err);
			return 1;
		}
	}
	if (!hostio.Initialize())
		return 1;

#if 0
	for (int i = 0; i < 1000; i++) {
		cpu.RunInstruction();
		cpu.Dump();
  }
return 1;

#endif

	unsigned int n = 0, m = 0;
	while (!hostio.Quitting()) {
		cpu.RunInstruction();
		cpu.Dump();
		if (++n >= 500) {
			vga.Update();
			hostio.Update();
			n = 0;
		}

		if (++m > 100) {
			memory.WriteWord(0x46c, memory.ReadWord(0x46c) + 1);
			m = 0;
		}
	}
	hostio.Cleanup();
	return 0;
}

/* vim:set ts=2 sw=2: */