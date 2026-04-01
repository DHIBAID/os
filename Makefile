all_c_sources := $(shell find . -name '*.c' -not -path './build/*' -not -path './dist/*' -not -path './targets/*' -printf '%P\n')
all_object_files := $(patsubst %.c, build/%.o, $(all_c_sources))

x86_64_asm_source_files := $(shell find . -name '*.asm' -not -path './build/*' -not -path './dist/*' -not -path './targets/*' -printf '%P\n')
x86_64_asm_object_files := $(patsubst %.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(x86_64_c_object_files) $(x86_64_asm_object_files)

build/%.o: %.c
	mkdir -p $(dir $@) && \
	x86_64-elf-gcc -c -I includes -ffreestanding -O0 -g $< -o $@

$(x86_64_asm_object_files): build/x86_64/%.o : %.asm
	mkdir -p $(dir $@) && \
	nasm -f elf64 -g -F dwarf $< -o $@

.PHONY: build-x86_64
build-x86_64: clean-x86_64 \
	$(all_object_files) $(x86_64_asm_object_files)
	mkdir -p dist/x86_64 && \
	x86_64-elf-ld -n -o dist/x86_64/kernel.elf \
		-T targets/x86_64/linker.ld \
		$(all_object_files) $(x86_64_asm_object_files) && \
	cp dist/x86_64/kernel.elf targets/x86_64/iso/boot/kernel.elf && \
	grub-mkrescue -o dist/x86_64/kernel.iso targets/x86_64/iso

clean-x86_64:
	$(SUDO) rm -rf build/*
	$(SUDO) rm -rf dist/x86_64
	$(SUDO) rm -rf targets/x86_64/iso/boot/kernel.elf

DISK_IMAGE := disk.img
MOUNT_POINT := mnt
DISK_SIZE_MB := 100
GRUB_INSTALL ?= false
QEMU_MEM ?= 1G

ifeq ($(shell id -u),0)
SUDO :=
else
SUDO := sudo
endif

.PHONY: disk-create disk-mount disk-unmount disk-add-files disk-list disk-inspect disk-clean install-to-disk run docker

disk-create:
	@echo "Creating $(DISK_SIZE_MB)MB disk image..."
	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=$(DISK_SIZE_MB)
	@echo "Creating partition table..."
	$(SUDO) parted $(DISK_IMAGE) mklabel msdos
	$(SUDO) parted $(DISK_IMAGE) mkpart primary fat32 1MiB 100%
	$(SUDO) parted $(DISK_IMAGE) set 1 boot on
	@echo "Formatting as FAT32..."
	$(SUDO) losetup -fP $(DISK_IMAGE)
	LOOP_DEV=$$($(SUDO) losetup -j $(DISK_IMAGE) | cut -d: -f1); \
	$(SUDO) mkfs.fat -F 32 $${LOOP_DEV}p1; \
	$(SUDO) losetup -d $${LOOP_DEV}
	@echo "Disk image created successfully!"

disk-mount:
	@mkdir -p $(MOUNT_POINT)
	$(SUDO) losetup -fP $(DISK_IMAGE)
	LOOP_DEV=$$($(SUDO) losetup -j $(DISK_IMAGE) | cut -d: -f1); \
	USER_ID=$$(id -u); GROUP_ID=$$(id -g); \
	$(SUDO) mount -o rw,uid=$${USER_ID},gid=$${GROUP_ID},umask=000 $${LOOP_DEV}p1 $(MOUNT_POINT); \
	echo "Disk mounted at $(MOUNT_POINT) (loop device: $${LOOP_DEV})"

disk-unmount:
	@LOOP_DEV=$$($(SUDO) losetup -j $(DISK_IMAGE) | cut -d: -f1); \
	$(SUDO) umount $(MOUNT_POINT) 2>/dev/null || true; \
	$(SUDO) losetup -d $${LOOP_DEV} 2>/dev/null || true; \
	echo "Disk unmounted"

disk-add-files: disk-mount
	@echo "Adding test files..."
	echo "Hello from FAT32 filesystem!" > $(MOUNT_POINT)/test.txt
	echo "This is a README file for testing." > $(MOUNT_POINT)/readme.txt
	echo "Boot loader test" > $(MOUNT_POINT)/boot.txt
	mkdir -p $(MOUNT_POINT)/docs
	echo "Documentation file" > $(MOUNT_POINT)/docs/info.txt
	$(MAKE) disk-unmount

disk-list: disk-mount
	@echo "Files on disk:"
	ls -lah $(MOUNT_POINT)
	$(MAKE) disk-unmount

disk-inspect:
	@echo "=== Disk Image Information ==="
	@ls -lh $(DISK_IMAGE) || true
	@echo ""
	$(SUDO) parted $(DISK_IMAGE) print || true
	@echo ""
	xxd -l 512 $(DISK_IMAGE) | head -20 || true

disk-clean:
	@$(MAKE) disk-unmount || true
	rm -f $(DISK_IMAGE)
	rm -rf $(MOUNT_POINT)

install-to-disk:
	@echo "Installing kernel to disk..."

	@if [ ! -f dist/x86_64/kernel.elf ]; then \
		echo "❌ Kernel not found. Run 'make build-x86_64'"; \
		exit 1; \
	fi

	@if [ ! -f $(DISK_IMAGE) ]; then \
		$(MAKE) disk-create; \
	fi

	@mkdir -p $(MOUNT_POINT)

	$(SUDO) losetup -fP $(DISK_IMAGE)
	LOOP=$$($(SUDO) losetup -j $(DISK_IMAGE) | cut -d: -f1); \
	$(SUDO) mount $${LOOP}p1 $(MOUNT_POINT); \
	mkdir -p $(MOUNT_POINT)/boot/grub; \
	$(SUDO) cp dist/x86_64/kernel.elf $(MOUNT_POINT)/boot/kernel.elf; \
	$(SUDO) cp targets/x86_64/grub.cfg $(MOUNT_POINT)/boot/grub/grub.cfg; \
	sync; \
	if [ "$(GRUB_INSTALL)" = "true" ]; then \
		echo "Installing GRUB..."; \
		$(SUDO) grub-install \
			--target=i386-pc \
			--boot-directory=$(MOUNT_POINT)/boot \
			--modules="part_msdos fat multiboot2" \
			--no-floppy \
			--recheck \
			$${LOOP}; \
		echo "GRUB installation finished"; \
	fi; \
	$(SUDO) umount $(MOUNT_POINT); \
	$(SUDO) losetup -d $${LOOP}

	@echo "Disk installation complete"

run:
	echo "Booting from hard disk..."; \
	qemu-system-x86_64 \
	    -S -s \
	    -m $(QEMU_MEM) \
	    -drive file=$(DISK_IMAGE),format=raw,if=ide


docker:
	$(SUDO) docker run --rm -it -v "$(PWD)":/root/env/ os-buildenv