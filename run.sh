#! /bin/bash

# Boot mode: cd (default) or hd (hard disk)
BOOT_MODE=${1:-cd}

if [ "$BOOT_MODE" = "hd" ]; then
    echo "Booting from hard disk..."
    qemu-system-x86_64 \
        -m 1G \
        -drive file=disk.img,format=raw,if=ide
else
    echo "Booting from CD-ROM..."
    qemu-system-x86_64 \
        -m 1G \
        -cdrom dist/x86_64/kernel.iso \
        -drive file=disk.img,format=raw,if=ide
fi