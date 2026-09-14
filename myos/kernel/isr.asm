global isr_timer
global isr_keyboard
global isr_page_fault

extern page_fault_handler


; ============================================================
; Page Fault ISR - Exception 14
;
; CPU automatically pushes:
;
;   error code
;   EIP
;   CS
;   EFLAGS
;
; We save the general-purpose registers and pass the CPU
; page-fault error code to the C handler.
;
; The C handler never returns.
; ============================================================

isr_page_fault:
    cli
    cld

    pusha

    ; pusha pushes 8 registers = 32 bytes.
    ;
    ; Therefore:
    ;
    ; [esp + 32] = CPU-provided page-fault error code

    mov eax, [esp + 32]

    push eax
    call page_fault_handler

.halt:
    cli
    hlt
    jmp .halt


; ============================================================
; Timer IRQ0
; ============================================================

isr_timer:
    pusha

    mov al, 0x20
    out 0x20, al

    popa
    iret


; ============================================================
; Keyboard IRQ1
; ============================================================

isr_keyboard:
    pusha

    mov al, 0x20
    out 0x20, al

    popa
    iret