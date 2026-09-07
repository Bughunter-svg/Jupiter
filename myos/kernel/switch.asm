section .text
global switch_task_asm

switch_task_asm:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov edx, [esp + 24]

    mov [eax + 24], esp

    mov esp, [edx + 24]

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret
