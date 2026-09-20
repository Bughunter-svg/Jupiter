bits 32

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

    mov ecx, [edx + 24]
    mov esp, ecx

    mov ecx, [edx + 40]
    mov cr3, ecx

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret