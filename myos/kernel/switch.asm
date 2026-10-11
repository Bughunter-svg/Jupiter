bits 32

section .text
global switch_task_asm
global switch_to_saved_context

switch_to_saved_context:
    mov edx, [esp + 4]
    mov ecx, [edx + 40]
    mov cr3, ecx
    mov ecx, [edx + 112]
    mov esp, ecx
    popa
    iretd

switch_task_asm:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov edx, [esp + 24]

    mov [eax + 24], esp
    mov dword [eax + 112], 0

    mov ecx, [edx + 24]
    test ecx, ecx
    jnz .cooperative

    mov ecx, [edx + 40]
    mov cr3, ecx

    mov ecx, [edx + 112]
    mov esp, ecx

    popa
    iretd

.cooperative:
    mov esp, ecx

    mov ecx, [edx + 40]
    mov cr3, ecx

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret
