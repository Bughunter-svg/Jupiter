bits 32

global isr_syscall

extern syscall_handler
extern process_exit

isr_syscall:
    cli
    cld
    pusha

    mov eax, [esp + 28]
    push eax
    call syscall_handler
    add esp, 4

    cmp eax, 1
    je .exit

.return_to_user:
    popa
    iret

.exit:
    call process_exit

.halt:
    cli
    hlt
    jmp .halt
