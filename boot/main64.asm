global long_mode_start
extern kernel_main

section .text
bits 64
long_mode_start:
    ; load null into all data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Mark this as the bottom of the stack for debuggers
    xor rbp, rbp
    push rbp        ; Push null return address (stack bottom marker)
    push rbp        ; Push null frame pointer
    mov rbp, rsp    ; Set up frame pointer

	call kernel_main
    hlt