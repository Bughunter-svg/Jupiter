section .text
global switch_task_asm

switch_task_asm:
    mov eax, [esp + 4]

    mov [eax + 0], eax
    mov [eax + 4], ebx
    mov [eax + 8], ecx
    mov [eax + 12], edx
    mov [eax + 16], esi
    mov [eax + 20], edi
    mov [eax + 24], esp
    mov [eax + 28], ebp

    mov ebx, [esp]
    mov [eax + 32], ebx

    pushf
    pop ebx
    mov [eax + 36], ebx

    mov eax, [esp + 8]

    mov ebx, [eax + 4]
    mov ecx, [eax + 8]
    mov edx, [eax + 12]
    mov esi, [eax + 16]
    mov edi, [eax + 20]
    mov esp, [eax + 24]
    mov ebp, [eax + 28]

    push dword [eax + 36]
    popf

    push dword [eax + 32]
    ret
