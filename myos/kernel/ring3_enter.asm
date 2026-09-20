bits 32

global enter_ring3

extern ring3_kernel_esp

enter_ring3:
    push ebp
    push edi
    push esi
    push ebx

    mov [ring3_kernel_esp], esp

    cli

    mov eax, [esp + 20]

    push 0x2B
    push eax
    push 0x202
    push 0x23
    push 0x00400000

    iretd
