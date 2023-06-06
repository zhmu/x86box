#!/bin/sh
qemu-img convert -O qcow2 floppy.img /tmp/f.img && qemu-system-i386 -hda /home/rink/vm/dos.img -m 8 -fda /tmp/f.img
