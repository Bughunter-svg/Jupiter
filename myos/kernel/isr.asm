bits 32

global isr_timer
global isr_keyboard
global isr_page_fault
global isr_general_protection

extern page_fault_handler
extern general_protection_handler
extern scheduler_tick

isr_page_fault:
    cli
    cld
    pusha

    mov eax, [esp + 32]
    push eax
    call page_fault_handler
    add esp, 4

    popa
    add esp, 4
    iret

isr_general_protection:
    cli
    cld
    pusha

    mov eax, [esp + 32]
    push eax
    call general_protection_handler
    add esp, 4

    popa
    add esp, 4
    iret

isr_timer:
    pusha
    mov al, 0x20
    out 0x20, al

    ; NEW: give the scheduler a chance to preempt on this tick.
    ; scheduler_tick() throttles itself internally and only calls
    ; yield() (switch_task_asm) every SCHEDULER_TICK_INTERVAL ticks.
    ; When it does switch away, execution of THIS task resumes right
    ; here (popa below) once it's scheduled back in, because every
    ; task that can be preempted this way was itself last suspended at
    ; exactly this same point in isr_timer - so the pusha/popa frames
    ; always line up.
    call scheduler_tick

    popa
    iret

isr_keyboard:
    pusha
    mov al, 0x20
    out 0x20, al
    popa
    iret