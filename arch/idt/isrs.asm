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
    cli
    call debug_exception_handler
    iretq