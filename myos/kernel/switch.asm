section .text
global switch_task_asm

switch_task_asm:
    mov eax, [esp + 4]
    mov edx, [esp + 8]

    mov [eax + 24], esp
    mov [eax + 28], ebp

    mov ecx, [esp]
    mov [eax + 32], ecx

    pushf
    pop ecx
    mov [eax + 36], ecx

    mov esp, [edx + 24]
    mov ebp, [edx + 28]

    push dword [edx + 36]
    popf

    push dword [edx + 32]
    ret
