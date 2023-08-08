#!/usr/bin/env python3
#
# This script will add padding and calculate the checksum of an option ROM
# image. This allows the option ROM to be used in x86emu, as the 8088_bios
# verifies the checksum and will ignore the option ROM if it is not correct
#
import sys

if len(sys.argv) != 3:
    print('usage: {} source.bin padded_checksummed.bin'.format(sys.argv[0]))
    quit()

source_file, dest_file = sys.argv[1:]

with open(source_file, 'rb') as f:
    bios = f.read()

expected_len = bios[2] * 512
print('expected_len %x' % (expected_len))
print('actual       %x' % (len(bios)))

bios = bytearray(bios)
while len(bios) < expected_len - 1:
    bios.append(0xff)

s = 0
for x in bios:
    s = (s + x) & 0xff
print('checksum     %x' % s)
bios.append(0x100 - s)

with open(dest_file, 'wb') as f:
    f.write(bios)
