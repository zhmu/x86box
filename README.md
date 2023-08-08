# x86box

[![ubuntu workflow status](https://github.com/zhmu/x86box/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/zhmu/x86box/actions?query=workflow%3Aubuntu)

## Introduction

`x86box` is a 80186/80188 emulator with support for PC peripherals. My intention is to use this project to gain a deeper understanding of the legacy PC architecture and peripherals.

## Peripherals

The following hardware is supposed to work:

- XT-IDE compatible parallel ATA
  - Hard drive size is a fixed 30MB at the moment
- Programmable DMA Controller (8237)
- Floppy drive controller (82077AA)
- Keyboard
- Programmable Interrupt Controller (8259A)
- Programmable Interval Timer (82C54)
- AT Real Time Clock
- VGA (80x25 text mode only)

## Usage

Building this project requires a C++20 compiler (GCC 11+ should work), CMake and the SDL2 and Capstone libraries. Ninja is recommended as the build tool (On Debian/Ubuntu, simply execute ``apt install build-essential ninja-build cmake libsdl2-dev libcapstone-dev``). Then you can build the software as follows:

```
$ cmake -B build -GNinja
$ cmake --build build
$ build/src/x86box --bios <path to bios.bin>
```

You'll need a suitable BIOS image - [micro_8088](https://github.com/skiselev/8088_bios) is a solid choice. The ``bios-micro8088.bin`` can be used.

If you want XT-IDE support, you can get the official [binaries](https://www.xtideuniversalbios.org/binaries/) - ``ide_xtl.bin`` is a good choice. Note that the checksum needs to be properly computed and the image must be padded to the correct side in order to be used. This can be performed using the ``checksum_pad_optionrom.py`` script as follows:

```
$ scripts/checksum_pad_optionrom.py ~/Downloads/ide_xtl.bin xtide.bin
```

With all these prerequites out of the way, you can launch ``x86box`` as follows:

``
$ build/src/x86box  --bios bios-micro8088.bin --rom xtide.bin
``

## Disk images

You can use ``--fd0 file.img`` to use ``file.img`` as an image for the first floppy drive (only a 1.44MB floppy image of exactly 1474560 bytes is supported). Multiple floppy images can be provided, _control-backtick_ (`, next to the number 1 on a standard keyboard) can be used to cycle between the images.

A hard drive image can be provided using ``--hd0 file.img``. This is currently expected to be a 31MB hard disk image of exactly 32117760 bytes (you can generate one using ``dd if=/dev/zero of=hdd.img bs=512 count=62730``)

## Testing

The `tests/` directory contains the testsuite of the emulator. This is intended to be developed alongside of the emulator, by making certain the currently supported hardware remains working properly.

## Goals

The following list is a _preliminary_ goal per version:

- Version 0.1: Boots MS-DOS 6.22 from floppy. Installs to a hard drive. Boots from hard drive.
- Version 0.2: Boots FreeDOS 1.3 from floppy. Installs to a hard drive. Boots from hard drive.
- Version 0.3: Runs Commander Keen 4
- Version 0.4: Runs Wolfenstein 3D (needs 286 support)
- Version 1.0: Runs DOOM (needs 386 support)

This list is by no means complete, and is intended as a guideline rather than a strict target. Perhaps some goals are unrealistic, or need extra tooling, support etc. This will be handled accordingly.

## License

All original work (in the `src/` and `test/` directories) is released using the terms of the `zlib` license as stated in the `LICENSE` file. External libraries (in the `external/` directory), disk images and other assets are released using their own terms.