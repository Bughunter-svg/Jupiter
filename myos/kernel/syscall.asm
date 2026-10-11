bits 32

global isr_syscall

extern syscall_handler
extern syscall_exit_pending
extern get_current_ring3_esp

isr_syscall:
    cli
    cld
    pusha

    push edx
    push ecx
    push ebx
    push eax
    call syscall_handler
    add esp, 16

    mov [esp + 28], eax

    call syscall_exit_pending
    cmp eax, 1
    je .exit

.return_to_user:
    popa
    iret

.exit:
    add esp, 32
    call get_current_ring3_esp
    mov esp, eax

    pop ebx
    pop esi
    pop edi
    pop ebp

    ret
