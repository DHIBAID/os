bits 16

section _TEXT class=CODE

global _x86_div64_32
_x86_div64_32:
    push bp
    mov bp, sp

    push bx

    mov eax, [bp + 8]
    mov ecx, [bp + 12]

    xor edx, edx
    div ecx

    mov bx, [bp + 16]
    mov [bx + 4], eax

    mov eax, [bp + 4]
    
    div ecx

    mov [bx], eax
    mov bx, [bp + 18]
    mov [bx], edx

    pop bx

    mov sp, bp
    pop bp
    ret




global _x86_Video_WriteCharTeletype
_x86_Video_WriteCharTeletype:
    push bp                ; Save base pointer
    mov bp, sp             ; Set base pointer to current stack pointer

    push bx                ; Save bx register

    mov ah, 0Eh            ; BIOS teletype function
    mov al, [bp + 4]       ; Load character to print into al
    mov bh, [bp + 6]       ; Load page number into bh
    int 10h                ; Call BIOS interrupt to print character

    pop bx                 ; Restore bx register
    mov sp, bp             ; Restore stack pointer
    pop bp                 ; Restore base pointer
    ret                    ; Return from function

