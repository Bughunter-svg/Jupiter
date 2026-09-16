#include "gdt.h"
#include "tss.h"
#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[7];
static struct gdt_ptr gdtr;

static void gdt_set_entry(
    int index,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t granularity)
{
    gdt[index].limit_low = (uint16_t)(limit & 0xFFFFU);
    gdt[index].base_low = (uint16_t)(base & 0xFFFFU);
    gdt[index].base_middle = (uint8_t)((base >> 16) & 0xFFU);
    gdt[index].access = access;
    gdt[index].granularity =
        (uint8_t)(((limit >> 16) & 0x0FU) |
                  (granularity & 0xF0U));
    gdt[index].base_high = (uint8_t)((base >> 24) & 0xFFU);
}

void gdt_set_tss_entry(uint32_t base, uint32_t limit)
{
    gdt_set_entry(6, base, limit, 0x89, 0x00);
}

void init_gdt(void)
{
    gdt_set_entry(0, 0, 0, 0x00, 0x00);
    gdt_set_entry(1, 0, 0, 0x00, 0x00);
    gdt_set_entry(2, 0, 0xFFFFFFFFU, 0x9A, 0xCF);
    gdt_set_entry(3, 0, 0xFFFFFFFFU, 0x92, 0xCF);
    gdt_set_entry(4, 0, 0xFFFFFFFFU, 0xFA, 0xCF);
    gdt_set_entry(5, 0, 0xFFFFFFFFU, 0xF2, 0xCF);

    tss_prepare();

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint32_t)&gdt;

    asm volatile(
        "lgdt (%0)\n"
        "ljmp $0x10, $1f\n"
        "1:\n"
        "mov $0x18, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :
        : "r"(&gdtr)
        : "ax", "memory"
    );
}