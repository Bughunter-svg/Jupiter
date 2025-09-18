#include "process.h"
#include "memory.h"
#include "screen.h"
#include <string.h>

ProcessControlBlock pcbs[MAX_PROCESSES];
int current_pid = 0;
int process_count = 1; // Kernel is process 0

extern void switch_task_asm(ProcessControlBlock* current, ProcessControlBlock* next);

void init_scheduler() {
    print("Initializing multi-tasking scheduler...\n");
    memset(pcbs, 0, sizeof(pcbs));
    
    // Kernel is process 0
    pcbs[0].pid = 0;
    pcbs[0].state = PROCESS_RUNNING;
    pcbs[0].priority = 0;
    strcpy(pcbs[0].name, "kernel");
    
    print("Scheduler ready. Max processes: ");
    print_int(MAX_PROCESSES);
    print("\n");
}

int create_process(void (*entry)(), const char* name, int priority) {
    if (process_count >= MAX_PROCESSES) {
        print("Error: Max processes reached!\n");
        return -1;
    }
    
    int pid = process_count++;
    ProcessControlBlock* pcb = &pcbs[pid];
    
    pcb->pid = pid;
    pcb->state = PROCESS_READY;
    pcb->priority = priority;
    pcb->eip = (uint32_t)entry;
    pcb->esp = (uint32_t)kmalloc(STACK_SIZE) + STACK_SIZE - 16;
    pcb->eflags = 0x202; // Interrupts enabled
    strcpy(pcb->name, name);
    
    // Initialize stack with entry point
    uint32_t* stack = (uint32_t*)pcb->esp;
    *(--stack) = pcb->eflags;
    *(--stack) = 0x08; // CS
    *(--stack) = (uint32_t)entry;
    
    print("Created process: ");
    print(name);
    print(" (PID: ");
    print_int(pid);
    print(")\n");
    
    return pid;
}

void yield() {
    // Simple round-robin scheduling
    int next_pid = (current_pid + 1) % process_count;
    
    while (pcbs[next_pid].state != PROCESS_READY) {
        next_pid = (next_pid + 1) % process_count;
        if (next_pid == current_pid) return; // No other ready processes
    }
    
    // Switch to next process
    ProcessControlBlock* current = &pcbs[current_pid];
    ProcessControlBlock* next = &pcbs[next_pid];
    
    current->state = PROCESS_READY;
    next->state = PROCESS_RUNNING;
    
    switch_task_asm(current, next);
    current_pid = next_pid;
}

void list_processes() {
    print("PID\tState\t\tName\n");
    print("--------------------------------\n");
    
    for (int i = 0; i < process_count; i++) {
        print_int(pcbs[i].pid);
        print("\t");
        
        switch (pcbs[i].state) {
            case PROCESS_READY: print("READY\t\t"); break;
            case PROCESS_RUNNING: print("RUNNING\t\t"); break;
            case PROCESS_BLOCKED: print("BLOCKED\t\t"); break;
            case PROCESS_ZOMBIE: print("ZOMBIE\t\t"); break;
        }
        
        print(pcbs[i].name);
        print("\n");
    }
}

int get_current_pid() {
    return current_pid;
}

