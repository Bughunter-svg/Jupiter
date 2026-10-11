#include "process.h"
#include "memory.h"
#include "screen.h"
#include "string.h"
#include "paging.h"
#include "vm.h"
#include "filesystem.h"

#include "ring3_binaries.inc"

ProcessControlBlock pcbs[MAX_PROCESSES];
int current_pid = 0;
int process_count = 1;

extern void switch_task_asm(ProcessControlBlock *current, ProcessControlBlock *next);
extern void process_trampoline(void);

extern void tss_set_kernel_stack(uint32_t stack_top);
extern void enter_ring3(uint32_t user_esp);
extern void *pmm_alloc_page(void);

void set_current_ring3_esp(uint32_t esp)
{
    pcbs[current_pid].ring3_esp = esp;
}

uint32_t get_current_ring3_esp(void)
{
    return pcbs[current_pid].ring3_esp;
}

#define RING3_CODE_VADDR      0x00400000U
#define RING3_STACK_VADDR     0x00800000U
#define RING3_STACK_SIZE      0x1000U
#define RING3_MAX_CODE_PAGES  16


static const unsigned char ring3_demo_code[] = {
    0xB8, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x5B, 0x00,
    0x40, 0x00, 0xB9, 0x13, 0x00, 0x00, 0x00, 0xCD,
    0x80, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x6E,
    0x00, 0x40, 0x00, 0xB9, 0x0A, 0x00, 0x00, 0x00,
    0xCD, 0x80, 0xB8, 0x03, 0x00, 0x00, 0x00, 0xCD,
    0x80, 0x04, 0x30, 0xA2, 0x78, 0x00, 0x40, 0x00,
    0xB8, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x78, 0x00,
    0x40, 0x00, 0xB9, 0x02, 0x00, 0x00, 0x00, 0xCD,
    0x80, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x00,
    0x00, 0x10, 0x00, 0xB9, 0x08, 0x00, 0x00, 0x00,
    0xCD, 0x80, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xCD,
    0x80, 0xEB, 0xFE, 0x48, 0x65, 0x6C, 0x6C, 0x6F,
    0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x72, 0x69,
    0x6E, 0x67, 0x20, 0x33, 0x21, 0x0A, 0x4D, 0x79,
    0x20, 0x50, 0x49, 0x44, 0x20, 0x69, 0x73, 0x20,
    0x30, 0x0A
};
#define SCHEDULER_TICK_INTERVAL 10
static int scheduler_tick_counter = 0;

void process_exit(void) {
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

    if (nxt->is_ring3)
        tss_set_kernel_stack(nxt->irq_stack + STACK_SIZE);

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

static uint32_t build_initial_frame(void *stack_mem) {
    uint32_t top = ((uint32_t)stack_mem + STACK_SIZE) & ~0xFUL;
    uint32_t *frame = (uint32_t *)(top - 44);
    int i;

    for (i = 0; i < 8; i++)
        frame[i] = 0;

    frame[8] = (uint32_t)process_trampoline;
    frame[9] = 0x10;
    frame[10] = 0x202;

    return (uint32_t)frame;
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
        int p;

        for (p = 0; p < pcb->user_code_num_pages; p++) {
            if (pcb->user_code_phys[p])
                pmm_free_page((void *)pcb->user_code_phys[p]);
        }

        if (pcb->user_stack_page)
            pmm_free_page((void *)pcb->user_stack_page);

        if (paging_destroy_address_space(pcb->cr3) != 0)
            return -4;
    }

    if (pcb->kernel_stack)
        kvfree((void *)pcb->kernel_stack);

    if (pcb->irq_stack)
        kvfree((void *)pcb->irq_stack);

    memset(pcb, 0, sizeof(ProcessControlBlock));
    pcb->state = PROCESS_UNUSED;
    pcb->pid = pid;

    return 0;
}

void process_trampoline(void) {
    ProcessControlBlock *pcb = &pcbs[current_pid];

    if (pcb->is_ring3) {
        tss_set_kernel_stack(pcb->irq_stack + STACK_SIZE);
        enter_ring3(pcb->user_esp);
        asm volatile("sti");
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

int process_name_exists(const char *name)
{
    for (int i = 0; i < process_count; i++) {
        if (pcbs[i].state != PROCESS_UNUSED &&
            strcmp(pcbs[i].name, name) == 0) {
            return 1;
        }
    }

    return 0;
}

int create_process(void (*entry)(void), const char *name, int priority) {
    if (!name || name[0] == '\0') {
        print("Error: process name cannot be empty.\n");
        return -1;
    }

    if (process_name_exists(name)) {
        print("Error: process '");
        print(name);
        print("' already exists.\n");
        return -1;
    }

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
    uint32_t initial_frame = build_initial_frame(stack_mem);
    
    pcb->eax = 0;
    pcb->ebx = 0;
    pcb->ecx = 0;
    pcb->edx = 0;
    pcb->esi = 0;
    pcb->edi = 0;
    pcb->esp = 0;
    pcb->irq_esp = initial_frame;
    pcb->ebp = 0;
    pcb->eip = (uint32_t)entry;
    pcb->eflags = 0x202;
    
    pcb->cr3 = paging_get_current_cr3();
    pcb->is_ring3 = 0;
    pcb->user_esp = 0;
    pcb->kernel_stack = (uint32_t)stack_mem;
    pcb->irq_stack = 0;
    pcb->ring3_esp = 0;
    pcb->user_code_page = 0;
    pcb->user_stack_page = 0;
    pcb->user_code_num_pages = 0;
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

static void free_ring3_code_pages(uint32_t *phys, int num_pages)
{
    int p;

    for (p = 0; p < num_pages; p++) {
        if (phys[p])
            pmm_free_page((void *)phys[p]);
    }
}

int create_ring3_program(const char *name, int priority, const unsigned char *code, unsigned int size) {
    void *stack_page;
    uint32_t kernel_cr3;
    uint32_t new_cr3;
    uint32_t user_stack_top;
    void *stack_mem;
    void *irq_mem;
    uint32_t initial_frame;
    int pid;
    ProcessControlBlock *pcb;
    unsigned int i;
    int num_pages;
    uint32_t code_phys[RING3_MAX_CODE_PAGES];
    int pages_allocated;
    int p;

    if (!code || size == 0 || size > RING3_MAX_CODE_PAGES * RING3_STACK_SIZE) {
        print("Error: invalid program size\n");
        return -1;
    }

    if (!name || name[0] == '\0') {
        print("Error: process name cannot be empty.\n");
        return -1;
    }

    if (process_name_exists(name)) {
        print("Error: process '");
        print(name);
        print("' already exists.\n");
        return -1;
    }

    num_pages = (int)((size + RING3_STACK_SIZE - 1) / RING3_STACK_SIZE);
    pages_allocated = 0;

    for (p = 0; p < RING3_MAX_CODE_PAGES; p++)
        code_phys[p] = 0;

    kernel_cr3 = paging_get_current_cr3();

    for (p = 0; p < num_pages; p++) {
        code_phys[p] = (uint32_t)pmm_alloc_page();

        if (!code_phys[p]) {
            print("Error: no memory for ring3 code pages\n");
            free_ring3_code_pages(code_phys, pages_allocated);
            return -1;
        }

        pages_allocated++;
    }

    stack_page = pmm_alloc_page();
    if (!stack_page) {
        print("Error: no memory for ring3 stack page\n");
        free_ring3_code_pages(code_phys, pages_allocated);
        return -1;
    }

    stack_mem = kvmalloc(STACK_SIZE);
    if (!stack_mem) {
        print("Error: no memory for process stack\n");
        free_ring3_code_pages(code_phys, pages_allocated);
        pmm_free_page(stack_page);
        return -1;
    }

    irq_mem = kvmalloc(STACK_SIZE);
    if (!irq_mem) {
        print("Error: no memory for interrupt stack\n");
        kvfree(stack_mem);
        free_ring3_code_pages(code_phys, pages_allocated);
        pmm_free_page(stack_page);
        return -1;
    }

    asm volatile("cli");

    new_cr3 = paging_create_address_space();
    if (!new_cr3) {
        print("Error: failed to create address space\n");
        kvfree(stack_mem);
        kvfree(irq_mem);
        free_ring3_code_pages(code_phys, pages_allocated);
        pmm_free_page(stack_page);
        asm volatile("sti");
        return -1;
    }

    if (paging_switch_address_space(new_cr3) != 0) {
        print("Error: failed to switch to process address space\n");
        kvfree(stack_mem);
        kvfree(irq_mem);
        free_ring3_code_pages(code_phys, pages_allocated);
        pmm_free_page(stack_page);
        paging_destroy_address_space(new_cr3);
        asm volatile("sti");
        return -1;
    }

    for (p = 0; p < num_pages; p++) {
        if (map_page(
                RING3_CODE_VADDR + (uint32_t)p * RING3_STACK_SIZE,
                code_phys[p],
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {

            print("Error: failed to map ring3 code page\n");
            paging_switch_address_space(kernel_cr3);
            asm volatile("sti");
            kvfree(stack_mem);
            kvfree(irq_mem);
            free_ring3_code_pages(code_phys, pages_allocated);
            pmm_free_page(stack_page);
            paging_destroy_address_space(new_cr3);
            return -1;
        }
    }

    if (map_page(RING3_STACK_VADDR, (uint32_t)stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
        print("Error: failed to map ring3 stack page\n");
        paging_switch_address_space(kernel_cr3);
        asm volatile("sti");
        kvfree(stack_mem);
        kvfree(irq_mem);
        free_ring3_code_pages(code_phys, pages_allocated);
        pmm_free_page(stack_page);
        paging_destroy_address_space(new_cr3);
        return -1;
    }

    {
        volatile unsigned char *dest = (volatile unsigned char *)RING3_CODE_VADDR;

        for (i = 0; i < size; i++)
            dest[i] = code[i];
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
        kvfree(stack_mem);
        kvfree(irq_mem);
        free_ring3_code_pages(code_phys, pages_allocated);
        pmm_free_page(stack_page);
        paging_destroy_address_space(new_cr3);
        return -1;
    }

    asm volatile("sti");

    pid = find_free_slot();

    if (pid < 0) {
        kvfree(stack_mem);
        kvfree(irq_mem);
        free_ring3_code_pages(code_phys, pages_allocated);
        pmm_free_page(stack_page);
        paging_destroy_address_space(new_cr3);
        return -1;
    }

    pcb = &pcbs[pid];

    initial_frame = build_initial_frame(stack_mem);

    pcb->eax = 0;
    pcb->ebx = 0;
    pcb->ecx = 0;
    pcb->edx = 0;
    pcb->esi = 0;
    pcb->edi = 0;
    pcb->esp = 0;
    pcb->irq_esp = initial_frame;
    pcb->ebp = 0;
    pcb->eip = RING3_CODE_VADDR;
    pcb->eflags = 0x202;
    pcb->cr3 = new_cr3;
    pcb->is_ring3 = 1;
    pcb->user_esp = user_stack_top;
    pcb->kernel_stack = (uint32_t)stack_mem;
    pcb->irq_stack = (uint32_t)irq_mem;
    pcb->ring3_esp = 0;
    pcb->user_code_page = code_phys[0];
    pcb->user_stack_page = (uint32_t)stack_page;
    pcb->user_code_num_pages = num_pages;

    for (p = 0; p < num_pages; p++)
        pcb->user_code_phys[p] = code_phys[p];

    for (p = num_pages; p < RING3_MAX_CODE_PAGES; p++)
        pcb->user_code_phys[p] = 0;

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

static int program_filename_valid(const char *filename)
{
    int i;

    if (!filename || !filename[0])
        return 0;

    for (i = 0; filename[i]; i++) {
        char c = filename[i];

        if (c == ' ' || c == '/' || c == '\\')
            return 0;
    }

    return 1;
}

int create_ring3_from_file(const char *filename)
{
    const unsigned char *data;
    unsigned int size;

    if (!program_filename_valid(filename)) {
        print("Error: invalid program name\n");
        return -1;
    }

    if (fs_read_binary(filename, &data, &size) != 0) {
        print("Error: program '");
        print(filename);
        print("' not found\n");
        return -1;
    }

    if (size == 0) {
        print("Error: program file is empty\n");
        return -1;
    }

    if (size > RING3_MAX_CODE_PAGES * RING3_STACK_SIZE) {
        print("Error: program too large (max 65536 bytes)\n");
        return -1;
    }

    return create_ring3_program(filename, 1, data, size);
}

static const unsigned char ring3_crash_null_code[] = {
    0xB8, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x18, 0x00,
    0x40, 0x00, 0xB9, 0x16, 0x00, 0x00, 0x00, 0xCD,
    0x80, 0xA1, 0x00, 0x00, 0x00, 0x00, 0xEB, 0xFE,
    0x41, 0x62, 0x6F, 0x75, 0x74, 0x20, 0x74, 0x6F,
    0x20, 0x72, 0x65, 0x61, 0x64, 0x20, 0x4E, 0x55,
    0x4C, 0x4C, 0x2E, 0x2E, 0x2E, 0x0A
};

static const unsigned char ring3_crash_kernel_code[] = {
    0xB8, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x1D, 0x00,
    0x40, 0x00, 0xB9, 0x23, 0x00, 0x00, 0x00, 0xCD,
    0x80, 0xC7, 0x05, 0x00, 0x00, 0x10, 0x00, 0x01,
    0x00, 0x00, 0x00, 0xEB, 0xFE, 0x41, 0x62, 0x6F,
    0x75, 0x74, 0x20, 0x74, 0x6F, 0x20, 0x77, 0x72,
    0x69, 0x74, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x6B,
    0x65, 0x72, 0x6E, 0x65, 0x6C, 0x20, 0x6D, 0x65,
    0x6D, 0x6F, 0x72, 0x79, 0x2E, 0x2E, 0x2E, 0x0A
};

static const unsigned char ring3_crash_priv_code[] = {
    0xB8, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x14, 0x00,
    0x40, 0x00, 0xB9, 0x29, 0x00, 0x00, 0x00, 0xCD,
    0x80, 0xFA, 0xEB, 0xFE, 0x41, 0x62, 0x6F, 0x75,
    0x74, 0x20, 0x74, 0x6F, 0x20, 0x72, 0x75, 0x6E,
    0x20, 0x61, 0x20, 0x70, 0x72, 0x69, 0x76, 0x69,
    0x6C, 0x65, 0x67, 0x65, 0x64, 0x20, 0x69, 0x6E,
    0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x69, 0x6F,
    0x6E, 0x2E, 0x2E, 0x2E, 0x0A
};

int create_ring3_process(const char *name, int priority) {
    return create_ring3_program(name, priority, ring3_demo_code, sizeof(ring3_demo_code));
}

static const unsigned char ring3_ticker_code[] = {
    0xB8, 0x03, 0x00, 0x00, 0x00, 0xCD, 0x80, 0x04,
    0x30, 0xA2, 0x55, 0x00, 0x40, 0x00, 0xBE, 0x05,
    0x00, 0x00, 0x00, 0x89, 0xF0, 0x04, 0x30, 0xA2,
    0x64, 0x00, 0x40, 0x00, 0xB8, 0x02, 0x00, 0x00,
    0x00, 0xBB, 0x50, 0x00, 0x40, 0x00, 0xB9, 0x16,
    0x00, 0x00, 0x00, 0xCD, 0x80, 0xB8, 0x04, 0x00,
    0x00, 0x00, 0xCD, 0x80, 0x89, 0xC7, 0x83, 0xC7,
    0x32, 0xB8, 0x04, 0x00, 0x00, 0x00, 0xCD, 0x80,
    0x39, 0xF8, 0x72, 0xF5, 0x4E, 0x75, 0xCC, 0xB8,
    0x01, 0x00, 0x00, 0x00, 0xCD, 0x80, 0xEB, 0xFE,
    0x5B, 0x70, 0x69, 0x64, 0x20, 0x30, 0x5D, 0x20,
    0x74, 0x69, 0x63, 0x6B, 0x73, 0x20, 0x6C, 0x65,
    0x66, 0x74, 0x3A, 0x20, 0x30, 0x0A
};

int create_ring3_ticker(const char *name) {
    return create_ring3_program(name, 1, ring3_ticker_code, sizeof(ring3_ticker_code));
}

void process_seed_program_files(void)
{
    fs_create_binary("hello", ring3_demo_code, (int)sizeof(ring3_demo_code));
    fs_create_binary("ticker", ring3_ticker_code, (int)sizeof(ring3_ticker_code));
    fs_create_binary("echo", ring3_echo_code, (int)sizeof(ring3_echo_code));
    fs_create_binary("big", ring3_big_code, (int)sizeof(ring3_big_code));
}

int create_ring3_crash(const char *name, int kind) {
    if (kind == 1)
        return create_ring3_program(name, 1, ring3_crash_kernel_code, sizeof(ring3_crash_kernel_code));

    if (kind == 2)
        return create_ring3_program(name, 1, ring3_crash_priv_code, sizeof(ring3_crash_priv_code));

    return create_ring3_program(name, 1, ring3_crash_null_code, sizeof(ring3_crash_null_code));
}

void user_fault(uint32_t vector, uint32_t error_code, uint32_t eip) {
    ProcessControlBlock *pcb = &pcbs[current_pid];
    uint32_t address;

    print("\nProcess ");
    print(pcb->name);
    print(" (PID ");
    print_int(current_pid);
    print(") killed: ");

    if (vector == 14) {
        asm volatile("mov %%cr2, %0" : "=r"(address));

        print("segmentation fault, ");

        if (error_code & 2)
            print("write to ");
        else
            print("read from ");

        print_hex(address);

        if (error_code & 1)
            print(" (protected)");
        else
            print(" (not mapped)");
    } else {
        print("general protection fault (code ");
        print_hex(error_code);
        print(")");
    }

    print(" at EIP ");
    print_hex(eip);
    print("\n");
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

    if (nxt->is_ring3)
        tss_set_kernel_stack(nxt->irq_stack + STACK_SIZE);

    switch_task_asm(cur, nxt);

    current_pid = cur->pid;
    cur->state = PROCESS_RUNNING;
}

uint32_t scheduler_tick(uint32_t irq_esp) {
    ProcessControlBlock *cur;
    ProcessControlBlock *nxt;
    int next;

    cur = &pcbs[current_pid];

    cur->irq_esp = irq_esp;
    cur->esp = 0;

    scheduler_tick_counter++;

    if (scheduler_tick_counter < SCHEDULER_TICK_INTERVAL)
        return irq_esp;

    scheduler_tick_counter = 0;

    next = (current_pid + 1) % process_count;

    while (pcbs[next].state != PROCESS_READY) {
        next = (next + 1) % process_count;

        if (next == current_pid)
            return irq_esp;
    }

    nxt = &pcbs[next];

    if (!nxt->irq_esp)
        return irq_esp;

    if (cur->state == PROCESS_RUNNING)
        cur->state = PROCESS_READY;
    nxt->state = PROCESS_RUNNING;

    current_pid = next;

    if (nxt->is_ring3)
        tss_set_kernel_stack(nxt->irq_stack + STACK_SIZE);

    if (nxt->cr3 != cur->cr3) {
        asm volatile(
            "mov %0, %%cr3"
            :
            : "r"(nxt->cr3)
            : "memory"
        );
    }

    return nxt->irq_esp;
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

int reap_zombies(void) {
    int count = 0;

    for (int i = 1; i < process_count; i++) {
        if (pcbs[i].state == PROCESS_ZOMBIE &&
            pcbs[i].parent_pid == current_pid) {
            if (reap_process(i) == 0)
                count++;
        }
    }

    return count;
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
