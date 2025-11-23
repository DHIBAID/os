#!/bin/bash

# Install kernel and GRUB bootloader to hard disk image
DISK_IMAGE="disk.img"
MOUNT_POINT="mnt"
KERNEL_BIN="dist/x86_64/kernel.bin"
INSTALL_GRUB=false

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        --grub)
            INSTALL_GRUB=true
            shift
            ;;
    esac
done

echo "=== Installing OS to Hard Disk with GRUB ==="

# Check if kernel exists
if [ ! -f "$KERNEL_BIN" ]; then
    echo "Error: Kernel not found at $KERNEL_BIN"
    echo "Run 'make build-x86_64' first"
    exit 1
fi

# Check if disk image exists
if [ ! -f "$DISK_IMAGE" ]; then
    echo "Error: Disk image not found. Creating one..."
    ./disk-manager.sh create
fi

if [ "$INSTALL_GRUB" = true ]; then
    # Check if grub-pc is installed
    if [ ! -d "/usr/lib/grub/i386-pc" ]; then
        echo "Error: GRUB for PC BIOS not found. Installing..."
        sudo apt install grub-pc-bin -y
    fi
fi

# Mount the disk
echo "Mounting disk..."
if ! mountpoint -q $MOUNT_POINT; then
    ./disk-manager.sh mount
fi

# Create boot directory structure
echo "Creating boot directory structure..."
mkdir -p $MOUNT_POINT/boot/grub

# Copy kernel
echo "Copying kernel..."
cp $KERNEL_BIN $MOUNT_POINT/boot/kernel.bin
echo "Kernel copied to /boot/kernel.bin"

if [ "$INSTALL_GRUB" = true ]; then
    # Create GRUB configuration
    echo "Creating GRUB configuration..."
    cat > $MOUNT_POINT/boot/grub/grub.cfg << 'EOF'
set timeout=0
set default=0

menuentry "My OS" {
    multiboot2 /boot/kernel.bin
    boot
}
EOF

    echo "GRUB configuration created"
fi

# Unmount
echo "Unmounting disk..."
./disk-manager.sh unmount

if [ "$INSTALL_GRUB" = true ]; then
    # Install GRUB to the disk image
    echo "Installing GRUB bootloader..."
    sudo losetup -fP $DISK_IMAGE
    LOOP_DEV=$(sudo losetup -j $DISK_IMAGE | cut -d: -f1)

    # Create temporary mount point for GRUB installation
    GRUB_MOUNT="/tmp/grub_install_$$"
    mkdir -p $GRUB_MOUNT
    sudo mount ${LOOP_DEV}p1 $GRUB_MOUNT

    # Install GRUB
    echo "Running grub-install on $LOOP_DEV..."
    sudo grub-install --target=i386-pc --boot-directory=$GRUB_MOUNT/boot --modules="part_msdos fat multiboot2" $LOOP_DEV

    # Verify GRUB installation
    if [ -d "$GRUB_MOUNT/boot/grub/i386-pc" ]; then
        echo "✓ GRUB installed successfully"
        echo "  Modules: $(ls $GRUB_MOUNT/boot/grub/i386-pc/*.mod | wc -l) files"
    else
        echo "✗ Warning: GRUB installation may have failed"
    fi

    # Cleanup
    sudo umount $GRUB_MOUNT
    rmdir $GRUB_MOUNT
    sudo losetup -d $LOOP_DEV
fi

echo ""
echo "=== Installation Complete! ==="
if [ "$INSTALL_GRUB" = true ]; then
    echo "Your OS is now bootable from $DISK_IMAGE with GRUB"
else
    echo "Kernel installed to $DISK_IMAGE (GRUB not installed, use --grub flag to install)"
fi
echo ""
echo "To test:"
echo "  ./run.sh hd     # Boot from hard disk"
echo ""
echo "Files installed:"
if [ "$INSTALL_GRUB" = true ]; then
    echo "  - Bootloader: GRUB (in MBR and /boot/grub)"
fi
echo "  - Kernel: /boot/kernel.bin"
