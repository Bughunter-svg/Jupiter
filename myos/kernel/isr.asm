global isr_timer
global isr_keyboard
global isr_page_fault

extern page_fault_handler

isr_page_fault:
    cli
    cld
    pusha

    ; CPU pushed the page-fault error code.
    ; pusha added 32 bytes.
    ; [esp + 32] = CPU error code.

    mov eax, [esp + 32]
    push eax
    call page_fault_handler
    add esp, 4

    ; Restore all registers saved by pusha.
    popa

    ; Now ESP points to the CPU-pushed error code.
    add esp, 4

    ; Return to the instruction that faulted.
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