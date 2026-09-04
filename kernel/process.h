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
    int          pid;
    ProcessState state;
    int          priority;
    char         name[32];
} ProcessControlBlock;

/* Scheduler API */
void init_scheduler(void);
int  create_process(void (*entry)(void), const char *name, int priority);
void switch_task(void);
void yield(void);
void list_processes(void);
int  get_current_pid(void);
int  get_process_count(void);   /* NEW – live count for status cmd   */

#endif /* PROCESS_H */
