#include "tss.h"
#include "gdt.h"
#include <stdint.h>

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static struct tss_entry tss;
static uint8_t kernel_stack[4096] __attribute__((aligned(16)));

void tss_prepare(void)
{
    uint32_t stack_top =
        (uint32_t)&kernel_stack[sizeof(kernel_stack)];

    tss.prev_tss = 0;
    tss.esp0 = stack_top;
    tss.ss0 = 0x18;

    tss.esp1 = 0;
    tss.ss1 = 0;
    tss.esp2 = 0;
    tss.ss2 = 0;

    tss.cr3 = 0;
    tss.eip = 0;
    tss.eflags = 0;

    tss.eax = 0;
    tss.ecx = 0;
    tss.edx = 0;
    tss.ebx = 0;
    tss.esp = 0;
    tss.ebp = 0;
    tss.esi = 0;
    tss.edi = 0;

    tss.es = 0x18;
    tss.cs = 0x10;
    tss.ss = 0x18;
    tss.ds = 0x18;
    tss.fs = 0x18;
    tss.gs = 0x18;

    tss.ldt = 0;
    tss.trap = 0;
    tss.iomap_base = sizeof(tss);

    gdt_set_tss_entry(
        (uint32_t)&tss,
        sizeof(tss) - 1
    );
}

void init_tss(void)
{
    asm volatile(
        "ltr %%ax"
        :
        : "a"((uint16_t)0x30)
        : "memory"
    );
}