bits 32

global isr_timer
global isr_keyboard
global isr_mouse
global isr_page_fault
global isr_general_protection
global isr_invalid_opcode

extern page_fault_handler
extern general_protection_handler
extern scheduler_tick
extern timer_handler
extern mouse_handler
extern user_fault
extern get_current_ring3_esp

isr_page_fault:
    cli
    cld
    pusha

    test dword [esp + 40], 3
    jnz .user_fault

    mov eax, [esp + 32]
    push eax
    call page_fault_handler
    add esp, 4

    popa
    add esp, 4
    iret

.user_fault:
    push dword [esp + 36]
    push dword [esp + 36]
    push dword 14
    call user_fault
    add esp, 12

    call get_current_ring3_esp
    mov esp, eax

    pop ebx
    pop esi
    pop edi
    pop ebp

    ret

isr_invalid_opcode:
    cli
    cld
    pusha

    test dword [esp + 40], 3
    jnz .user_fault

    mov eax, [esp + 32]
    push eax
    call general_protection_handler
    add esp, 4

    popa
    iret

.user_fault:
    push dword [esp + 36]
    push dword [esp + 36]
    push dword 6
    call user_fault
    add esp, 12

    call get_current_ring3_esp
    mov esp, eax

    pop ebx
    pop esi
    pop edi
    pop ebp

    ret

isr_general_protection:
    cli
    cld
    pusha

    test dword [esp + 40], 3
    jnz .user_fault

    mov eax, [esp + 32]
    push eax
    call general_protection_handler
    add esp, 4

    popa
    add esp, 4
    iret

.user_fault:
    push dword [esp + 36]
    push dword [esp + 36]
    push dword 13
    call user_fault
    add esp, 12

    call get_current_ring3_esp
    mov esp, eax

    pop ebx
    pop esi
    pop edi
    pop ebp

    ret

isr_timer:
    pusha

    mov al, 0x20
    out 0x20, al

    call timer_handler

    push esp
    call scheduler_tick
    add esp, 4

    mov esp, eax
    popa
    iret

isr_keyboard:
    pusha

    mov al, 0x20
    out 0x20, al

    popa
    iret

isr_mouse:
    pusha
    cld

    call mouse_handler

    mov al, 0x20
    out 0xA0, al
    out 0x20, al

    popa
    iret
