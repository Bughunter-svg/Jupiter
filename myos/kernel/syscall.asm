bits 32

global isr_syscall

extern syscall_handler
extern ring3_kernel_esp

isr_syscall:
    cli
    cld
    pusha

    call syscall_handler

    cmp eax, 1
    je .exit

.return_to_user:
    popa
    iret

.exit:
    add esp, 32
    mov esp, [ring3_kernel_esp]

    pop ebx
    pop esi
    pop edi
    pop ebp

    sti
    ret