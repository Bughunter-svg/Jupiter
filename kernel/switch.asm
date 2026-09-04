section .text
global switch_task_asm

switch_task_asm:
    pushad

    mov eax, [esp + 36]
    mov edx, [esp + 40]

    mov [eax + 4], ebx
    mov [eax + 8], ecx
    mov [eax + 12], edx
    mov [eax + 16], esi
    mov [eax + 20], edi
    mov [eax + 24], esp
    mov [eax + 28], ebp

    mov ecx, [esp + 32]
    mov [eax + 32], ecx

    pushf
    pop ecx
    mov [eax + 36], ecx

    mov eax, [esp + 40]

    mov ebx, [eax + 4]
    mov ecx, [eax + 8]
    mov edx, [eax + 12]
    mov esi, [eax + 16]
    mov edi, [eax + 20]
    mov esp, [eax + 24]
    mov ebp, [eax + 28]

    push dword [eax + 36]
    popf

    mov ecx, [eax + 32]
    push ecx

    popad
    ret
