global start

section .text
bits 32
start:
    ; print something

    mov dword [0xB8000], 0x2f4b2f4f ; 'ok'
    hlt ; cpu to freeze