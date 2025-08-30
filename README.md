# Custom 64-bit Operating System

A minimal 64-bit operating system written from scratch in C and Assembly, targeting the x86_64 architecture. This project demonstrates fundamental OS concepts including bootloading, memory management, keyboard input, display output, and a basic command-line interface.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Core Components](#core-components)
- [Build System](#build-system)
- [Development Environment](#development-environment)
- [Usage](#usage)
- [Commands](#commands)
- [Technical Details](#technical-details)
- [Building and Running](#building-and-running)

## Overview

This operating system is designed as an educational project to understand low-level system programming and OS development. It boots from GRUB, sets up a 64-bit environment, initializes memory management, and provides a basic terminal interface for user interaction.

## Features

- **64-bit Long Mode**: Boots into x86_64 long mode with proper CPU feature detection
- **Memory Management**: Physical memory manager with frame allocation and kernel heap
- **Display Driver**: VGA text mode with color support and scrolling
- **Keyboard Driver**: PS/2 keyboard input with scancode translation
- **Terminal Interface**: Basic shell with command parsing and execution
- **GRUB Bootloader**: Uses GRUB2 for multiboot compliance

## Architecture

The OS follows a modular layered architecture:

```
┌─────────────────────────────┐
│        User Interface      │  ← Terminal/Shell
├─────────────────────────────┤
│         Kernel Layer       │  ← Memory Management, Process Control
├─────────────────────────────┤
│      Hardware Drivers      │  ← Display, Keyboard, Memory
├─────────────────────────────┤
│     Hardware Abstraction   │  ← Low-level I/O, Assembly routines
├─────────────────────────────┤
│        Boot Sequence       │  ← GRUB → 32-bit → 64-bit transition
└─────────────────────────────┘
```

## Project Structure

```
os/
├── src/                          # Source code
│   ├── implementations/          # Implementation files
│   │   ├── kernel/              # Kernel core functionality
│   │   │   ├── kernel.c         # Main kernel entry point
│   │   │   ├── memory.c         # Memory management implementation
│   │   │   └── terminal.c       # Terminal/shell implementation
│   │   └── x86_64/              # x86_64 specific code
│   │       ├── boot/            # Boot sequence
│   │       │   ├── header.asm   # Multiboot header
│   │       │   ├── main.asm     # 32-bit boot code
│   │       │   └── main64.asm   # 64-bit initialization
│   │       ├── drivers/         # Hardware drivers
│   │       │   ├── display.c    # VGA display driver
│   │       │   └── keyboard.c   # PS/2 keyboard driver
│   │       ├── printf.c         # Print functions
│   │       ├── string.c         # String utilities
│   │       └── util.c           # General utilities
│   └── interface/               # Header files/interfaces
│       ├── printf.h             # Print function declarations
│       ├── string.h             # String function declarations
│       └── util.h               # Utility function declarations
├── targets/                     # Target-specific files
│   └── x86_64/
│       ├── linker.ld            # Linker script
│       └── iso/                 # ISO build directory
├── build/                       # Compiled object files
├── buildenv/                    # Docker build environment
│   └── Dockerfile              # Cross-compilation environment
├── Makefile                     # Build configuration
└── dist/                        # Final build artifacts
```

## Core Components

### 1. Boot Sequence (`src/implementations/x86_64/boot/`)

#### **header.asm**
- Defines the multiboot2 header required by GRUB
- Sets up the entry point for the bootloader

#### **main.asm** 
- **CPU Feature Detection**: Checks for multiboot compliance, CPUID support, and long mode capability
- **Page Table Setup**: Creates identity mapping for the first 2MB of memory
- **Long Mode Transition**: Enables paging and switches from 32-bit to 64-bit mode
- **GDT Configuration**: Sets up Global Descriptor Table for 64-bit operation

#### **main64.asm**
- **64-bit Initialization**: Sets up segment registers and stack for 64-bit operation
- **Kernel Handoff**: Calls the main kernel entry point in C

### 2. Memory Management (`src/implementations/kernel/memory.c`)

#### **Physical Memory Manager (PMM)**
- **Frame Allocation**: Manages 4KB physical memory frames using a bitmap
- **Bitmap Tracking**: Efficient bit-level tracking of allocated/free frames
- **Memory Statistics**: Provides system memory usage information

**Key Functions:**
- `pmm_init()`: Initializes the physical memory manager with total system memory
- `pmm_alloc_frame()`: Allocates a 4KB physical memory frame
- `pmm_free_frame()`: Frees a previously allocated frame
- `meminfo()`: Displays detailed memory usage statistics

#### **Kernel Heap Manager**
- **Bump Allocator**: Simple linear allocator for kernel memory allocation
- **8-byte Alignment**: Ensures proper memory alignment for performance
- **Size Tracking**: Monitors total heap allocation

**Key Functions:**
- `kheap_init()`: Initializes the kernel heap at a specified address
- `kmalloc()`: Allocates memory from the kernel heap
- Memory tracking and reporting capabilities

### 3. Display Driver (`src/implementations/x86_64/drivers/display.c`)

#### **VGA Text Mode Driver**
- **80x25 Character Display**: Standard VGA text mode with 2000 character positions
- **16-Color Support**: Full VGA color palette for foreground and background
- **Hardware Cursor**: Real-time cursor positioning using VGA registers
- **Screen Scrolling**: Automatic scrolling when reaching bottom of screen

**Key Functions:**
- `put_char_at()`: Places a character at specific coordinates with color
- `scroll_screen()`: Scrolls screen content up by one line
- `remove_char_at_cursor()`: Handles backspace functionality
- Direct VGA hardware manipulation using port I/O

### 4. Keyboard Driver (`src/implementations/x86_64/drivers/keyboard.c`)

#### **PS/2 Keyboard Interface**
- **Scancode Translation**: Converts PS/2 scancodes to ASCII characters
- **Key State Management**: Tracks key press/release states to prevent repeats
- **Special Key Handling**: Proper handling of backspace, enter, and other special keys
- **Polling-based Input**: Efficient polling mechanism for character input

**Key Functions:**
- `bios_get_char()`: Main interface for getting keyboard input
- `read_from_usb_keyboard()`: Low-level keyboard reading with state management
- `usb_read()`: Hardware-level scancode reading from port 0x60
- Complete scancode-to-ASCII translation table

### 5. Terminal Interface (`src/implementations/kernel/terminal.c`)

#### **Command-Line Shell**
- **Command Parsing**: Interprets user input and executes commands
- **Input Buffer Management**: Safe input handling with overflow protection
- **Command History**: Basic command processing with feedback
- **Error Handling**: Graceful handling of unknown commands

**Supported Commands:**
- `clear`: Clear the terminal screen
- `help`: Display available commands and usage
- `reboot`: Restart the system using BIOS interrupt
- `shutdown`: Shutdown the system (QEMU/hardware specific)
- `echo <message>`: Display a message
- `meminfo`: Show detailed memory usage statistics

### 6. Print System (`src/implementations/x86_64/printf.c`)

#### **Formatted Output**
- **Character Output**: Individual character printing with position tracking
- **String Output**: Null-terminated string printing
- **Numeric Output**: Decimal number formatting and display
- **Color Management**: Dynamic foreground/background color control
- **Line Management**: Automatic line wrapping and newline handling

**Key Functions:**
- `print_char()`: Output single character with automatic positioning
- `print_str()`: Output null-terminated strings
- `print_dec()`: Output decimal numbers
- `print_set_color()`: Configure text colors
- `print_clear()`: Clear entire screen

### 7. Utility Functions (`src/implementations/x86_64/string.c`, `util.c`)

#### **String Operations**
- `strcmp()`: String comparison
- `strncmp()`: Length-limited string comparison
- `strlen()`: String length calculation

#### **System Utilities**
- `sleep()`: Basic delay functionality
- `outw()`: 16-bit port output operations
- Low-level system utility functions

## Build System

### **Makefile Configuration**
The build system uses GNU Make with cross-compilation support:

- **Source Discovery**: Automatically finds all `.c` and `.asm` files
- **Cross-Compilation**: Uses `x86_64-elf-gcc` and `nasm` for proper targeting
- **Dependency Management**: Automatic dependency resolution and incremental builds
- **ISO Generation**: Creates bootable ISO images using GRUB

### **Docker Build Environment**
- **Consistent Environment**: Reproducible builds across different host systems
- **Cross-Compilation Tools**: Pre-configured with `gcc-cross-x86_64-elf`
- **Required Tools**: NASM assembler, GRUB tools, and xorriso for ISO creation

## Technical Details

### **Memory Layout**
- **Boot Location**: Kernel loaded at 1MB (0x100000)
- **Heap Start**: Kernel heap begins at 4MB (0x400000)
- **Frame Size**: 4KB (4096 bytes) physical memory frames
- **Page Tables**: Identity mapping for the first 2MB

### **Hardware Interface**
- **VGA Buffer**: Direct access to 0xB8000 for text mode display
- **Keyboard Controller**: PS/2 controller at port 0x60
- **VGA Cursor**: Controlled via ports 0x3D4 and 0x3D5
- **System Control**: Basic ACPI and BIOS interrupt support

### **Boot Process**
1. **GRUB Loading**: GRUB loads the kernel binary and passes control
2. **Multiboot Verification**: Confirms proper bootloader handoff
3. **CPU Feature Detection**: Verifies CPUID and long mode support
4. **Page Table Setup**: Creates minimal paging structures
5. **Long Mode Activation**: Switches to 64-bit mode
6. **Kernel Initialization**: Sets up memory management and drivers
7. **Terminal Activation**: Starts the command-line interface

## Building and Running

### **Prerequisites**
- Docker (for consistent build environment)
- QEMU (for testing and emulation)

### **Build Commands**
```bash
# Build the Docker environment
./docker.sh

# Compile the OS
./make.sh

# Run in QEMU
./run.sh
```

### **Manual Build Process**
```bash
# Enter build environment
docker run --rm -it -v "$(pwd)":/root/env buildenv

# Compile the kernel
make build-x86_64

# The resulting ISO will be in dist/x86_64/kernel.iso
```

### **Running the OS**
```bash
# Using QEMU
qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso

# Or use the provided script
./run.sh
```

## Commands

Once the OS boots, you'll see a terminal prompt `/>:`. Available commands:

- **`help`**: Display all available commands and their descriptions
- **`clear`**: Clear the terminal screen
- **`echo <message>`**: Print the specified message
- **`meminfo`**: Display detailed memory usage statistics including:
  - Total memory frames
  - Free memory frames
  - Memory usage percentage
  - Heap allocation details
- **`reboot`**: Restart the system
- **`shutdown`**: Power down the system (works in QEMU)

---

This operating system represents a complete, minimal implementation suitable for educational purposes and as a foundation for more complex operating system features.
