bits 32

global isr_timer
global isr_keyboard
global isr_page_fault
global isr_general_protection

extern page_fault_handler
extern general_protection_handler

isr_page_fault:
    cli
    cld
    pusha

    mov eax, [esp + 32]
    push eax
    call page_fault_handler
    add esp, 4

    popa
    add esp, 4
    iret

isr_general_protection:
    cli
    cld
    pusha

    mov eax, [esp + 32]
    push eax
    call general_protection_handler
    add esp, 4

    popa
    add esp, 4
    iret

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