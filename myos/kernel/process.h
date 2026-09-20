#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 16
#define STACK_SIZE    4096

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE
} ProcessState;

typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, esp, ebp;
    uint32_t eip, eflags, cr3;
    uint32_t user_esp;
    int          pid;
    ProcessState state;
    int          priority;
    char         name[32];
    int          is_ring3;   /* NEW - if set, process_trampoline calls
                                 enter_ring3(user_esp) instead of eip() */
} ProcessControlBlock;

/* Scheduler API */
void init_scheduler(void);
int  create_process(void (*entry)(void), const char *name, int priority);

/*
 * NEW - builds an isolated address space, maps a tiny demo ring-3
 * program + user stack into it, and registers it as a normal
 * schedulable process. See process.c for the caveat about TSS.esp0
 * being shared across ring-3 processes: safe for one at a time, not
 * yet safe for true concurrent preemption of several.
 */
int  create_ring3_process(const char *name, int priority);

void switch_task(void);
void yield(void);
void list_processes(void);
int  get_current_pid(void);
int  get_process_count(void);   /* NEW – live count for status cmd   */

/*
 * NEW - called from isr_timer on every timer tick. Internally
 * throttles to a fixed interval before actually calling yield(), so
 * the timer ISR can call it unconditionally every tick.
 */
void scheduler_tick(void);

#endif /* PROCESS_H */