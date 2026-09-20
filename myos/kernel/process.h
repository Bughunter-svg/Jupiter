#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 16
#define STACK_SIZE    4096

typedef enum {
    PROCESS_UNUSED,
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
    int          is_ring3;
    uint32_t     kernel_stack;
    uint32_t     user_code_page;
    uint32_t     user_stack_page;
    int          parent_pid;
} ProcessControlBlock;

void init_scheduler(void);
int  create_process(void (*entry)(void), const char *name, int priority);
int  create_ring3_process(const char *name, int priority);
void switch_task(void);
void yield(void);
void list_processes(void);
int  get_current_pid(void);
int  get_process_count(void);
int  wait_process(int pid);
void scheduler_tick(void);

#endif
