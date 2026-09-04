global isr_timer
global isr_keyboard

isr_timer:
    pusha
    mov al, 0x20
    out 0x20, al
    popa
    iret

isr_keyboard:
    pusha
    mov al, 0x20
    out 0x20, al
    popa
    iret
