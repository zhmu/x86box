#!/bin/sh
qemu-img convert -O raw /tmp/f.img /tmp/raw.img && mcopy -i /tmp/raw.img ::/out.bin .
