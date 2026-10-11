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
    uint32_t     irq_esp;
    uint32_t     irq_stack;
    uint32_t     ring3_esp;
    int          user_code_num_pages;
    uint32_t     user_code_phys[16];
    int          job_background;
} ProcessControlBlock;

void init_scheduler(void);
int  create_process(void (*entry)(void), const char *name, int priority);
int  create_ring3_process(const char *name, int priority);
int  create_ring3_program(const char *name, int priority, const unsigned char *code, unsigned int size);
int  create_ring3_from_file(const char *filename);
void process_seed_program_files(void);
int  create_ring3_crash(const char *name, int kind);
int  create_ring3_ticker(const char *name);
void user_fault(uint32_t vector, uint32_t error_code, uint32_t eip);
void switch_task(void);
void yield(void);
void list_processes(void);
void show_process_info(int pid);
void process_set_background(int pid, int background);
void list_shell_jobs(void);
int  get_current_pid(void);
int  get_process_count(void);
int  wait_process(int pid);
int  reap_zombies(void);
uint32_t scheduler_tick(uint32_t irq_esp);
void set_current_ring3_esp(uint32_t esp);
uint32_t get_current_ring3_esp(void);

#endif
