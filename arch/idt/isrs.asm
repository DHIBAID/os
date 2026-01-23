; Definition of each IDT entry

; ISR0: Divide by zero exception
extern divide_by_zero_handler
global isr_divide_by_zero

isr_divide_by_zero:
    cli                             ; Clear interrupt register
    call divide_by_zero_handler     ; Call the C handler
    iretq                           ; Return from interrupt (can't be reached since we halt in the handler)