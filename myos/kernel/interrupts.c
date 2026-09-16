#include "interrupts.h"
#include "ports.h"
#include "screen.h"

#define KERNEL_CODE_SELECTOR 0x10

struct idt_entry {
    unsigned short base_lo;
    unsigned short sel;
    unsigned char always0;
    unsigned char flags;
    unsigned short base_hi;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void isr_syscall(void);
extern void isr_timer(void);
extern void isr_keyboard(void);
extern void isr_page_fault(void);
extern void isr_general_protection(void);

static void idt_set_gate(unsigned char num,
                         unsigned long base,
                         unsigned short sel,
                         unsigned char flags)
{
    idt[num].base_lo = (unsigned short)(base & 0xFFFF);
    idt[num].base_hi = (unsigned short)((base >> 16) & 0xFFFF);
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

static void init_idt(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned int)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(
        13,
        (unsigned long)isr_general_protection,
        KERNEL_CODE_SELECTOR,
        0x8E
    );

    idt_set_gate(
        14,
        (unsigned long)isr_page_fault,
        KERNEL_CODE_SELECTOR,
        0x8E
    );

    idt_set_gate(
        0x20,
        (unsigned long)isr_timer,
        KERNEL_CODE_SELECTOR,
        0x8E
    );

    idt_set_gate(
        0x21,
        (unsigned long)isr_keyboard,
        KERNEL_CODE_SELECTOR,
        0x8E
    );

    idt_set_gate(
        0x80,
        (unsigned long)isr_syscall,
        KERNEL_CODE_SELECTOR,
        0xEE
    );

    asm volatile(
        "lidtl (%0)"
        :
        : "r"(&idtp)
        : "memory"
    );
}

void init_interrupts(void)
{
    asm volatile("cli");

    init_idt();

    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);

    print("PIC remapped - ONLY timer enabled\n");
}

void enable_interrupts(void)
{
    asm volatile("sti");
}

void disable_interrupts(void)
{
    asm volatile("cli");
}

void debug_interrupts(void)
{
    print("Interrupt Debug:\n");

    print("IDT Base: ");
    print_hex((unsigned int)&idt);
    print("\n");

    print("PIC Mask: ");
    print_hex(inb(0x21));
    print("\n");
}
