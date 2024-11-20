org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

; since we added a function before 'main', we jump to main directly
start:
    jmp main

; print a string to the screen
; Params:
;   - ds:si points to string
puts:
    ; save registers we will modify
    push si
    push ax

.loop:
    lodsb               ; loads next character in al
    or al, al           ; verify if next char is null
    jz .done            ; if al is zero, we are done
    
    mov ah, 0x0E        ; call bios interrupt
    mov bh, 0
    int 0x10
 
    jmp .loop

.done:
    ; restore registers
    pop ax
    pop si
    ret

main:

    ; setup data segments
    mov ax, 0           ; can't write to ds/es directly
    mov ds, ax
    mov es, ax

    mov ss, ax
    mov sp, 0x7C00      ; stack grows downwards from where we are loaded in memory

    ; print message
    mov si, message
    call puts


    hlt

.halt:
    jmp .halt


message: db 'Hello World', ENDL, 0

times 510-($-$$) db 0
dw 0AA55h