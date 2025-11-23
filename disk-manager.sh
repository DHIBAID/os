#!/bin/bash

# Disk image manager for OS development
DISK_IMAGE="disk.img"
MOUNT_POINT="mnt"
DISK_SIZE_MB=100

create_disk() {
    echo "Creating ${DISK_SIZE_MB}MB disk image..."
    dd if=/dev/zero of=$DISK_IMAGE bs=1M count=$DISK_SIZE_MB
    
    echo "Creating partition table..."
    sudo parted $DISK_IMAGE mklabel msdos
    sudo parted $DISK_IMAGE mkpart primary fat32 1MiB 100%
    sudo parted $DISK_IMAGE set 1 boot on
    
    echo "Formatting as FAT32..."
    sudo losetup -fP $DISK_IMAGE
    LOOP_DEV=$(sudo losetup -j $DISK_IMAGE | cut -d: -f1)
    sudo mkfs.fat -F 32 ${LOOP_DEV}p1
    sudo losetup -d $LOOP_DEV
    
    echo "Disk image created successfully!"
}

mount_disk() {
    if [ ! -f "$DISK_IMAGE" ]; then
        echo "Error: $DISK_IMAGE not found. Run './disk-manager.sh create' first."
        exit 1
    fi
    
    mkdir -p $MOUNT_POINT
    sudo losetup -fP $DISK_IMAGE
    LOOP_DEV=$(sudo losetup -j $DISK_IMAGE | cut -d: -f1)
    
    # Mount with read-write permissions for current user
    USER_ID=$(id -u)
    GROUP_ID=$(id -g)
    sudo mount -o rw,uid=$USER_ID,gid=$GROUP_ID,umask=000 ${LOOP_DEV}p1 $MOUNT_POINT
    
    echo "Disk mounted at $MOUNT_POINT (loop device: $LOOP_DEV)"
    echo "You can now add/modify files in $MOUNT_POINT as your user"
    echo "Owner: $(id -un):$(id -gn)"
}

unmount_disk() {
    if [ ! -d "$MOUNT_POINT" ]; then
        echo "Mount point doesn't exist"
        exit 1
    fi
    
    LOOP_DEV=$(sudo losetup -j $DISK_IMAGE | cut -d: -f1)
    sudo umount $MOUNT_POINT 2>/dev/null || true
    sudo losetup -d $LOOP_DEV 2>/dev/null || true
    echo "Disk unmounted"
}

add_test_files() {
    if ! mountpoint -q $MOUNT_POINT; then
        echo "Disk not mounted. Mounting first..."
        mount_disk
    fi
    
    echo "Adding test files..."
    echo "Hello from FAT32 filesystem!" > $MOUNT_POINT/test.txt
    echo "This is a README file for testing." > $MOUNT_POINT/readme.txt
    echo "Boot loader test" > $MOUNT_POINT/boot.txt
    
    mkdir -p $MOUNT_POINT/docs
    echo "Documentation file" > $MOUNT_POINT/docs/info.txt
    
    echo "Test files added!"
    list_files
}

list_files() {
    if ! mountpoint -q $MOUNT_POINT; then
        echo "Disk not mounted. Mounting first..."
        mount_disk
    fi
    
    echo "Files on disk:"
    ls -lah $MOUNT_POINT
}

inspect_disk() {
    echo "=== Disk Image Information ==="
    echo "File: $DISK_IMAGE"
    if [ -f "$DISK_IMAGE" ]; then
        ls -lh $DISK_IMAGE
        echo ""
        echo "=== Partition Table ==="
        sudo parted $DISK_IMAGE print
        echo ""
        echo "=== MBR Hex Dump (first 512 bytes) ==="
        xxd -l 512 $DISK_IMAGE | head -20
    else
        echo "Disk image not found!"
    fi
}

show_usage() {
    echo "Disk Image Manager for OS Development"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  create       - Create a new FAT32 disk image"
    echo "  mount        - Mount the disk image"
    echo "  unmount      - Unmount the disk image"
    echo "  add-files    - Add test files to the disk"
    echo "  list         - List files on the disk"
    echo "  inspect      - Show disk information"
    echo "  clean        - Unmount and remove disk image"
    echo ""
    echo "Example workflow:"
    echo "  $0 create"
    echo "  $0 mount"
    echo "  $0 add-files"
    echo "  $0 unmount"
}

clean() {
    echo "Cleaning up..."
    unmount_disk
    rm -f $DISK_IMAGE
    rm -rf $MOUNT_POINT
    echo "Cleanup complete"
}

# Main script
case "$1" in
    create)
        create_disk
        ;;
    mount)
        mount_disk
        ;;
    unmount)
        unmount_disk
        ;;
    add-files)
        add_test_files
        unmount_disk
        ;;
    list)
        list_files
        ;;
    inspect)
        inspect_disk
        ;;
    clean)
        clean
        ;;
    *)
        show_usage
        exit 1
        ;;
esac
