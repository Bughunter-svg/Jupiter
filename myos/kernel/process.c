#include "process.h"
#include "memory.h"
#include "screen.h"
#include "string.h"

ProcessControlBlock pcbs[MAX_PROCESSES];
int current_pid   = 0;
int process_count = 1;   /* kernel is process 0 */

extern void switch_task_asm(ProcessControlBlock *current, ProcessControlBlock *next);

void init_scheduler(void) {
    print("Initializing scheduler...\n");
    memset(pcbs, 0, sizeof(pcbs));

    /* Kernel is PID 0 */
    pcbs[0].pid      = 0;
    pcbs[0].state    = PROCESS_RUNNING;
    pcbs[0].priority = 0;
    /* Use our own strcpy from string.c */
    int i = 0;
    const char *n = "kernel";
    while (n[i]) { pcbs[0].name[i] = n[i]; i++; }
    pcbs[0].name[i] = '\0';

    print("Scheduler ready. Max processes: ");
    print_int(MAX_PROCESSES);
    print("\n");
}

int create_process(void (*entry)(void), const char *name, int priority) {
    if (process_count >= MAX_PROCESSES) {
        print("Error: Max processes reached!\n");
        return -1;
    }

    int pid = process_count++;
    ProcessControlBlock *pcb = &pcbs[pid];

    void *stack_mem = kmalloc(STACK_SIZE);
    if (!stack_mem) {
        print("Error: No memory for process stack\n");
        process_count--;
        return -1;
    }

    uint32_t stack_top = (uint32_t)stack_mem + STACK_SIZE;
    stack_top -= sizeof(uint32_t);

    *((uint32_t *)stack_top) = (uint32_t)entry;

    pcb->eax     = 0;
    pcb->ebx     = 0;
    pcb->ecx     = 0;
    pcb->edx     = 0;
    pcb->esi     = 0;
    pcb->edi     = 0;
    pcb->esp     = stack_top;
    pcb->ebp     = 0;
    pcb->eip     = (uint32_t)entry;
    pcb->eflags  = 0x202;
    pcb->cr3     = 0;
    pcb->pid     = pid;
    pcb->state   = PROCESS_READY;
    pcb->priority = priority;

    int i = 0;
    while (name[i] && i < 31) {
        pcb->name[i] = name[i];
        i++;
    }
    pcb->name[i] = '\0';

    print("Created process: ");
    print(name);
    print(" (PID ");
    print_int(pid);
    print(")\n");

    return pid;
}

void yield(void) {
    int next = (current_pid + 1) % process_count;

    while (pcbs[next].state != PROCESS_READY) {
        next = (next + 1) % process_count;

        if (next == current_pid)
            return;
    }

    ProcessControlBlock *cur = &pcbs[current_pid];
    ProcessControlBlock *nxt = &pcbs[next];

    cur->state = PROCESS_READY;
    nxt->state = PROCESS_RUNNING;

    current_pid = next;

    switch_task_asm(cur, nxt);

    current_pid = cur->pid;
    cur->state = PROCESS_RUNNING;
}

void list_processes(void) {
    print("PID  STATE    PRIORITY  NAME\n");
    print("--------------------------------\n");
    for (int i = 0; i < process_count; i++) {
        char buf[4];
        itoa(pcbs[i].pid, buf, 10);
        print(buf); print("    ");
        switch (pcbs[i].state) {
            case PROCESS_READY:   print("READY    "); break;
            case PROCESS_RUNNING: print("RUNNING  "); break;
            case PROCESS_BLOCKED: print("BLOCKED  "); break;
            case PROCESS_ZOMBIE:  print("ZOMBIE   "); break;
        }
        itoa(pcbs[i].priority, buf, 10);
        print(buf); print("         ");
        print(pcbs[i].name); print("\n");
    }
}

int get_current_pid(void)   { return current_pid; }
int get_process_count(void) { return process_count; }
