#include "process.h"
#include "memory.h"
#include "screen.h"
#include "string.h"
#include "paging.h"
#include "vm.h"

ProcessControlBlock pcbs[MAX_PROCESSES];
int current_pid = 0;
int process_count = 1;

extern void switch_task_asm(ProcessControlBlock *current, ProcessControlBlock *next);
extern void process_trampoline(void);


extern void enter_ring3(uint32_t user_esp);
extern void *pmm_alloc_page(void);

#define RING3_CODE_VADDR   0x00400000U
#define RING3_STACK_VADDR  0x00401000U
#define RING3_STACK_SIZE   0x1000U


static const unsigned char ring3_demo_code[] = {
    0xB8, 0x37, 0x13, 0x00, 0x00,
    0xCD, 0x80,
    0xB8, 0x01, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xEB, 0xFE
};
#define SCHEDULER_TICK_INTERVAL 10   
static int scheduler_tick_counter = 0;

static void process_exit(void) {
    ProcessControlBlock *pcb = &pcbs[current_pid];
    int next = -1;

    print("\nProcess ");
    print(pcb->name);
    print(" exited.\n");

    pcb->state = PROCESS_ZOMBIE;

    for (int i = 0; i < process_count; i++) {
        if (pcbs[i].state == PROCESS_READY) {
            next = i;
            break;
        }
    }

    if (next < 0) {
        for (;;) {
            asm volatile("cli\nhlt");
        }
    }

    ProcessControlBlock *nxt = &pcbs[next];

    nxt->state = PROCESS_RUNNING;
    current_pid = next;

    switch_task_asm(pcb, nxt);

    for (;;) {
        asm volatile("cli\nhlt");
    }
}

static int find_free_slot(void) {
    for (int i = 1; i < process_count; i++) {
        if (pcbs[i].state == PROCESS_UNUSED)
            return i;
    }

    if (process_count < MAX_PROCESSES)
        return process_count++;

    return -1;
}

static int active_processes(void) {
    int count = 0;

    for (int i = 0; i < process_count; i++) {
        if (pcbs[i].state != PROCESS_UNUSED)
            count++;
    }

    return count;
}

static int reap_process(int pid) {
    ProcessControlBlock *pcb;

    if (pid <= 0 || pid >= process_count)
        return -1;

    pcb = &pcbs[pid];

    if (pcb->state != PROCESS_ZOMBIE)
        return -2;

    if (pcb->parent_pid != current_pid)
        return -3;

    if (pcb->is_ring3) {
        if (pcb->user_code_page)
            pmm_free_page((void *)pcb->user_code_page);

        if (pcb->user_stack_page)
            pmm_free_page((void *)pcb->user_stack_page);

        if (paging_destroy_address_space(pcb->cr3) != 0)
            return -4;
    }

    if (pcb->kernel_stack)
        kvfree((void *)pcb->kernel_stack);

    memset(pcb, 0, sizeof(ProcessControlBlock));
    pcb->state = PROCESS_UNUSED;
    pcb->pid = pid;

    return 0;
}

void process_trampoline(void) {
    ProcessControlBlock *pcb = &pcbs[current_pid];

    if (pcb->is_ring3) {
        
        enter_ring3(pcb->user_esp);
    } else {
        void (*entry)(void) = (void (*)(void))pcb->eip;
        entry();
    }

    print("\n>>> TRAMPOLINE: ENTRY RETURNED <<<\n");

    process_exit();
}

void init_scheduler(void) {
    print("Initializing scheduler...\n");

    memset(pcbs, 0, sizeof(pcbs));

    for (int i = 0; i < MAX_PROCESSES; i++)
        pcbs[i].state = PROCESS_UNUSED;

    pcbs[0].pid = 0;
    pcbs[0].state = PROCESS_RUNNING;
    pcbs[0].priority = 0;
    pcbs[0].cr3 = paging_get_current_cr3();
    pcbs[0].is_ring3 = 0;

    int i = 0;
    const char *n = "kernel";

    while (n[i]) {
        pcbs[0].name[i] = n[i];
        i++;
    }

    pcbs[0].name[i] = '\0';

    print("Scheduler ready. Max processes: ");
    print_int(MAX_PROCESSES);
    print("\n");
}

int create_process(void (*entry)(void), const char *name, int priority) {
    int pid = find_free_slot();

    if (pid < 0) {
        print("Error: Max processes reached!\n");
        return -1;
    }

    ProcessControlBlock *pcb = &pcbs[pid];

    void *stack_mem = kvmalloc(STACK_SIZE);

    if (!stack_mem) {
        print("Error: No memory for process stack\n");
        if (pid == process_count - 1 && pid > 0)
            process_count--;
        return -1;
    }
    uint32_t stack_top = (uint32_t)stack_mem + STACK_SIZE;
    stack_top &= ~0xFUL;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = (uint32_t)process_trampoline;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;
    
    pcb->eax = 0;
    pcb->ebx = 0;
    pcb->ecx = 0;
    pcb->edx = 0;
    pcb->esi = 0;
    pcb->edi = 0;
    pcb->esp = stack_top;
    pcb->ebp = 0;
    pcb->eip = (uint32_t)entry;
    pcb->eflags = 0x202;
    
    pcb->cr3 = paging_get_current_cr3();
    pcb->is_ring3 = 0;
    pcb->user_esp = 0;
    pcb->kernel_stack = (uint32_t)stack_mem;
    pcb->user_code_page = 0;
    pcb->user_stack_page = 0;
    pcb->parent_pid = current_pid;
    pcb->pid = pid;
    pcb->state = PROCESS_READY;
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

int create_ring3_process(const char *name, int priority) {
    void *code_page;
    void *stack_page;
    uint32_t kernel_cr3;
    uint32_t new_cr3;
    uint32_t user_stack_top;
    void *stack_mem;
    uint32_t stack_top;
    int pid;
    ProcessControlBlock *pcb;
    unsigned int i;

    kernel_cr3 = paging_get_current_cr3();

    code_page = pmm_alloc_page();
    if (!code_page) {
        print("Error: no memory for ring3 code page\n");
        return -1;
    }

    stack_page = pmm_alloc_page();
    if (!stack_page) {
        print("Error: no memory for ring3 stack page\n");
        pmm_free_page(code_page);
        return -1;
    }

    stack_mem = kvmalloc(STACK_SIZE);
    if (!stack_mem) {
        print("Error: no memory for process stack\n");
        pmm_free_page(code_page);
        pmm_free_page(stack_page);
        return -1;
    }

    asm volatile("cli");

    new_cr3 = paging_create_address_space();
    if (!new_cr3) {
        print("Error: failed to create address space\n");
        kvfree(stack_mem);
        pmm_free_page(code_page);
        pmm_free_page(stack_page);
        asm volatile("sti");
        return -1;
    }

    if (paging_switch_address_space(new_cr3) != 0) {
        print("Error: failed to switch to process address space\n");
        kvfree(stack_mem);
        pmm_free_page(code_page);
        pmm_free_page(stack_page);
        return -1;
    }

    if (map_page(RING3_CODE_VADDR, (uint32_t)code_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
        print("Error: failed to map ring3 code page\n");
        paging_switch_address_space(kernel_cr3);
        asm volatile("sti");
        kvfree(stack_mem);
        pmm_free_page(code_page);
        pmm_free_page(stack_page);
        return -1;
    }

    if (map_page(RING3_STACK_VADDR, (uint32_t)stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
        print("Error: failed to map ring3 stack page\n");
        paging_switch_address_space(kernel_cr3);
        asm volatile("sti");
        kvfree(stack_mem);
        pmm_free_page(code_page);
        pmm_free_page(stack_page);
        return -1;
    }

    {
        volatile unsigned char *dest = (volatile unsigned char *)RING3_CODE_VADDR;
        for (i = 0; i < sizeof(ring3_demo_code); i++)
            dest[i] = ring3_demo_code[i];
    }

    {
        volatile unsigned char *dest = (volatile unsigned char *)RING3_STACK_VADDR;
        for (i = 0; i < RING3_STACK_SIZE; i++)
            dest[i] = 0;
    }

    user_stack_top = RING3_STACK_VADDR + RING3_STACK_SIZE - 16;

    if (paging_switch_address_space(kernel_cr3) != 0) {
        print("Error: failed to restore kernel address space\n");
        asm volatile("sti");
        return -1;
    }

    asm volatile("sti");

    pid = find_free_slot();

    if (pid < 0) {
        kvfree(stack_mem);
        pmm_free_page(code_page);
        pmm_free_page(stack_page);
        paging_destroy_address_space(new_cr3);
        return -1;
    }

    pcb = &pcbs[pid];

    stack_top = (uint32_t)stack_mem + STACK_SIZE;
    stack_top &= ~0xFUL;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = (uint32_t)process_trampoline;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;
    stack_top -= sizeof(uint32_t);
    *((uint32_t *)stack_top) = 0;

    pcb->eax = 0;
    pcb->ebx = 0;
    pcb->ecx = 0;
    pcb->edx = 0;
    pcb->esi = 0;
    pcb->edi = 0;
    pcb->esp = stack_top;
    pcb->ebp = 0;
    pcb->eip = RING3_CODE_VADDR;
    pcb->eflags = 0x202;
    pcb->cr3 = new_cr3;
    pcb->is_ring3 = 1;
    pcb->user_esp = user_stack_top;
    pcb->kernel_stack = (uint32_t)stack_mem;
    pcb->user_code_page = (uint32_t)code_page;
    pcb->user_stack_page = (uint32_t)stack_page;
    pcb->parent_pid = current_pid;
    pcb->pid = pid;
    pcb->state = PROCESS_READY;
    pcb->priority = priority;

    i = 0;
    while (name[i] && i < 31) {
        pcb->name[i] = name[i];
        i++;
    }
    pcb->name[i] = '\0';

    print("Created ring3 process: ");
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

void scheduler_tick(void) {
    scheduler_tick_counter++;

    if (scheduler_tick_counter < SCHEDULER_TICK_INTERVAL)
        return;

    scheduler_tick_counter = 0;

    
    yield();
}

void list_processes(void) {
    print("PID  STATE    PRIORITY  NAME\n");
    print("--------------------------------\n");

    for (int i = 0; i < process_count; i++) {
        char buf[4];

        if (pcbs[i].state == PROCESS_UNUSED)
            continue;

        itoa(pcbs[i].pid, buf, 10);

        print(buf);
        print("    ");

        switch (pcbs[i].state) {
            case PROCESS_READY:
                print("READY    ");
                break;

            case PROCESS_RUNNING:
                print("RUNNING  ");
                break;

            case PROCESS_BLOCKED:
                print("BLOCKED  ");
                break;

            case PROCESS_ZOMBIE:
                print("ZOMBIE   ");
                break;

            case PROCESS_UNUSED:
                print("UNUSED   ");
                break;
        }

        itoa(pcbs[i].priority, buf, 10);

        print(buf);
        print("         ");
        print(pcbs[i].name);
        print("\n");
    }
}

int get_current_pid(void) {
    return current_pid;
}

int get_process_count(void) {
    return active_processes();
}

int wait_process(int pid) {
    ProcessControlBlock *pcb;

    if (pid <= 0 || pid >= process_count)
        return -1;

    pcb = &pcbs[pid];

    if (pcb->parent_pid != current_pid)
        return -3;

    if (pcb->state == PROCESS_UNUSED)
        return -1;

    for (;;) {
        if (pcb->state == PROCESS_ZOMBIE)
            return reap_process(pid);

        if (pcb->state == PROCESS_UNUSED)
            return -1;

        yield();
    }
}