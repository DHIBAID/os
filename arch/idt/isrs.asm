; Definition of each IDT entry

; ISR0: Divide by zero exception
extern divide_by_zero_handler
global isr_divide_by_zero

isr_divide_by_zero:
    cli                             ; Clear interrupt register
    call divide_by_zero_handler     ; Call the C handler
    iretq                           ; Return from interrupt (can't be reached since we halt in the handler)


; ISR1: Debug exception
extern debug_exception_handler
global isr_debug_exception

isr_debug_exception:
    ; CPU already pushed:
    ;   RIP
    ;   CS
    ;   RFLAGS
    ; RSP points to RIP

    ; Save volatile registers (SysV ABI)
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; At this point stack is misaligned.
    ; We will realign before call.

    ; Allocate debug_state_t

    sub rsp, 48                ; space for debug_state_t (6 * 8 = 48 bytes)

    ; Snapshot debug registers

    mov rax, dr0
    mov [rsp + 0], rax

    mov rax, dr1
    mov [rsp + 8], rax

    mov rax, dr2
    mov [rsp + 16], rax

    mov rax, dr3
    mov [rsp + 24], rax

    mov rax, dr6
    mov [rsp + 32], rax

    mov rax, dr7
    mov [rsp + 40], rax


    ; Setup arguments

    ; rdi = interrupt_frame*
    lea rdi, [rsp + 48 + 9*8]   ; skip debug_state + saved regs

    ; rsi = debug_state*
    mov rsi, rsp

    ; Align stack (must be 16-byte aligned before call)
    sub rsp, 8

    call debug_exception_handler

    add rsp, 8

    ; Restore debug registers

    mov rax, [rsp + 0]
    mov dr0, rax

    mov rax, [rsp + 8]
    mov dr1, rax

    mov rax, [rsp + 16]
    mov dr2, rax

    mov rax, [rsp + 24]
    mov dr3, rax

    mov rax, [rsp + 32]
    mov dr6, rax

    mov rax, [rsp + 40]
    mov dr7, rax

    add rsp, 48

    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    iretq

; ISR2: Non-Maskable Interrupt (NMI) Exception
extern nmi_exception_handler
global isr_nmi_exception

isr_nmi_exception:

    ; CPU has pushed (ring0 case):
    ;   RIP
    ;   CS
    ;   RFLAGS

    ; --- Establish a proper frame ---
    push rbp
    mov rbp, rsp

    ; --- Save all general purpose registers ---
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Stack alignment:
    ;
    ; CPU push: 24 bytes
    ; push rbp: 8 bytes
    ; 14 GPRs: 112 bytes
    ;
    ; Total = 24 + 8 + 112 = 144 bytes
    ; 144 % 16 = 0 → aligned

    ; Pass pointer to saved registers (top of stack)
    mov rdi, rsp
    call nmi_exception_handler

    ; --- Restore registers in reverse order ---
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; --- Restore frame ---
    pop rbp

    iretq
