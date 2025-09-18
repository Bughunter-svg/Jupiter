; kernel/switch.asm
section .text
global switch_task_asm

switch_task_asm:
    ; Save current process state
    mov eax, [esp + 4]  ; current PCB pointer
    mov [eax + 0], ebx
    mov [eax + 4], ecx
    mov [eax + 8], edx
    mov [eax + 12], esi
    mov [eax + 16], edi
    mov [eax + 20], esp
    mov [eax + 24], ebp
    
    ; Save instruction pointer and flags
    pushf
    pop dword [eax + 32]  ; eflags
    mov ebx, [esp]        ; return address (eip)
    mov [eax + 28], ebx   ; eip
    
    ; Load next process state
    mov eax, [esp + 8]    ; next PCB pointer
    mov ebx, [eax + 0]
    mov ecx, [eax + 4]
    mov edx, [eax + 8]
    mov esi, [eax + 12]
    mov edi, [eax + 16]
    mov esp, [eax + 20]
    mov ebp, [eax + 24]
    
    ; Load instruction pointer and flags
    push dword [eax + 32] ; eflags
    popf
    push dword [eax + 28] ; eip
    
    ret
