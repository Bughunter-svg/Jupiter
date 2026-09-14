#include "interrupts.h"
#include "ports.h"
#include "screen.h"

/*
 * JupiterOS currently uses the GDT provided by the boot environment.
 *
 * QEMU shows that the kernel is executing with:
 *
 *     CS = 0x10
 *     SS = 0x18
 *
 * Therefore IDT interrupt gates must use 0x10 as their
 * kernel code-segment selector.
 */
#define KERNEL_CODE_SELECTOR 0x10

struct idt_entry {
    unsigned short base_lo;
    unsigned short sel;
    unsigned char  always0;
    unsigned char  flags;
    unsigned short base_hi;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void isr_timer(void);
extern void isr_keyboard(void);
extern void isr_page_fault(void);


/* ============================================================
 * IDT Gate Setup
 * ============================================================ */

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


/* ============================================================
 * Initialize IDT
 * ============================================================ */

static void init_idt(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned int)&idt;

    /*
     * Start with every vector disabled.
     */
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /*
     * Exception 14 = Page Fault
     *
     * 0x8E = present, ring 0, 32-bit interrupt gate.
     */
    idt_set_gate(
        14,
        (unsigned long)isr_page_fault,
        KERNEL_CODE_SELECTOR,
        0x8E
    );

    /*
     * IRQ0 = Timer
     */
    idt_set_gate(
        0x20,
        (unsigned long)isr_timer,
        KERNEL_CODE_SELECTOR,
        0x8E
    );

    /*
     * IRQ1 = Keyboard
     */
    idt_set_gate(
        0x21,
        (unsigned long)isr_keyboard,
        KERNEL_CODE_SELECTOR,
        0x8E
    );

    /*
     * Load the IDT.
     */
    asm volatile(
        "lidtl (%0)"
        :
        : "r"(&idtp)
        : "memory"
    );
}


/* ============================================================
 * Interrupt Initialization
 * ============================================================ */

void init_interrupts(void)
{
    /*
     * Interrupts remain disabled while we configure
     * the IDT and PIC.
     */
    asm volatile("cli");

    init_idt();

    /*
     * Remap the 8259 PIC.
     *
     * Master IRQs  -> 0x20 - 0x27
     * Slave IRQs   -> 0x28 - 0x2F
     */

    /* ICW1 */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /* ICW2 */
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    /* ICW3 */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    /* ICW4 */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /*
     * Only IRQ0 (timer) is currently enabled.
     *
     * 11111110 = 0xFE
     *
     * NOTE:
     * We leave IRQ1 disabled here, matching your existing
     * safe-mode behavior.
     */
    outb(0x21, 0xFE);

    /*
     * Disable every slave IRQ.
     */
    outb(0xA1, 0xFF);

    print("PIC remapped - ONLY timer enabled\n");
}


/* ============================================================
 * Global Interrupt Control
 * ============================================================ */

void enable_interrupts(void)
{
    asm volatile("sti");
}

void disable_interrupts(void)
{
    asm volatile("cli");
}


/* ============================================================
 * Debug
 * ============================================================ */

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