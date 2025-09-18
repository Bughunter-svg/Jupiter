#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 16
#define STACK_SIZE 4096

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING, 
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE
} ProcessState;

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, esp, ebp, eip, eflags;
    uint32_t cr3;
    int pid;
    ProcessState state;
    int priority;
    char name[32];
} ProcessControlBlock;

// Function declarations
void init_scheduler();
int create_process(void (*entry)(), const char* name, int priority);
void switch_task();
void yield();
void list_processes();
int get_current_pid();

#endif
