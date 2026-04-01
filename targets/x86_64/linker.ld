ENTRY(start)

SECTIONS
{
  . = 1M;

  .boot :
  {
    KEEP(*(.multiboot_header))
  }

  .text :
  {
    *(.text)
  }

  .rodata :
  {
    *(.rodata*)
  }

  .data :
  {
    *(.data*)
  }

  .bss :
  {
    *(COMMON)
    *(.bss*)
    . = ALIGN(16);
  }

  kernel_end = .;
}