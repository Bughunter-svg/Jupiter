#include "paging.h"
#include "memory.h"
#include "screen.h"
#include <stdint.h>

#define PAGE_ENTRIES    1024U
#define PAGE_FRAME_MASK 0xFFFFF000U

/* Page-fault error-code bits */
#define PAGE_FAULT_PROTECTION    0x001U
#define PAGE_FAULT_WRITE         0x002U
#define PAGE_FAULT_USER          0x004U
#define PAGE_FAULT_RESERVED_BIT  0x008U
#define PAGE_FAULT_INSTRUCTION   0x010U

/* Deliberate page-fault test address */
#define PAGE_FAULT_TEST_ADDRESS   0x00800000U

static uint32_t page_directory[PAGE_ENTRIES]
    __attribute__((aligned(4096)));

static uint32_t first_page_table[PAGE_ENTRIES]
    __attribute__((aligned(4096)));


/*
 * Page Fault Handler
 */

void page_fault_handler(uint32_t error_code)
{
    uint32_t fault_address;
    void *physical_page;

    /*
     * CR2 contains the virtual address that caused the page fault.
     */
    asm volatile(
        "mov %%cr2, %0"
        : "=r"(fault_address)
    );

    print("\n");
    print("================================\n");
    print("           PAGE FAULT\n");
    print("================================\n");

    print("Fault address: ");
    print_hex(fault_address);
    print("\n");

    if (error_code & PAGE_FAULT_PROTECTION)
        print("Type: PROTECTION VIOLATION\n");
    else
        print("Type: NOT PRESENT\n");

    if (error_code & PAGE_FAULT_WRITE)
        print("Access: WRITE\n");
    else
        print("Access: READ\n");

    if (error_code & PAGE_FAULT_USER)
        print("Mode: USER\n");
    else
        print("Mode: KERNEL\n");

    if (error_code & PAGE_FAULT_RESERVED_BIT)
        print("Reserved-bit violation: YES\n");

    if (error_code & PAGE_FAULT_INSTRUCTION)
        print("Instruction fetch: YES\n");

    /*
     * Recover only the deliberate test fault.
     *
     * This prevents the kernel from blindly allocating memory
     * for arbitrary page faults.
     */
    if (fault_address == PAGE_FAULT_TEST_ADDRESS &&
        !(error_code & PAGE_FAULT_PROTECTION)) {

        physical_page = pmm_alloc_page();

        if (physical_page) {
            if (map_page(
                    fault_address,
                    (uint32_t)physical_page,
                    PAGE_PRESENT | PAGE_WRITABLE) == 0) {

                print("Page allocated and mapped.\n");
                print("Page fault recovered.\n");

                /*
                 * Return to the ISR.
                 *
                 * The CPU will retry the faulting instruction.
                 */
                return;
            }

            /*
             * Mapping failed, so return the allocated
             * physical page to the PMM.
             */
            pmm_free_page(physical_page);
        }

        print("Page fault recovery FAILED.\n");
    }

    /*
     * Any unexpected page fault is fatal for now.
     */
    print("System halted.\n");

    for (;;) {
        asm volatile(
            "cli\n"
            "hlt"
        );
    }
}


/*
 * Return whether paging is currently enabled.
 */

int paging_is_enabled(void)
{
    uint32_t cr0;

    asm volatile(
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    return (cr0 & 0x80000000U) != 0;
}


/*
 * Get page-table entry for a virtual address.
 *
 * Returns NULL if the corresponding page table
 * does not exist.
 */

uint32_t *get_page(uint32_t virtual_addr)
{
    uint32_t directory_index;
    uint32_t table_index;
    uint32_t page_table;

    directory_index = virtual_addr >> 22;
    table_index = (virtual_addr >> 12) & 0x3FFU;

    if (!(page_directory[directory_index] & PAGE_PRESENT))
        return 0;

    page_table =
        page_directory[directory_index] & PAGE_FRAME_MASK;

    return &((uint32_t *)page_table)[table_index];
}


/*
 * Map one virtual page to one physical page.
 */

int map_page(uint32_t virtual_addr,
             uint32_t physical_addr,
             uint32_t flags)
{
    uint32_t directory_index;
    uint32_t table_index;
    uint32_t *page_table;
    void *new_table;

    /*
     * Work with page-aligned addresses.
     */
    virtual_addr &= PAGE_FRAME_MASK;
    physical_addr &= PAGE_FRAME_MASK;

    directory_index = virtual_addr >> 22;
    table_index = (virtual_addr >> 12) & 0x3FFU;

    /*
     * Allocate a page table if one doesn't exist.
     */
    if (!(page_directory[directory_index] & PAGE_PRESENT)) {
        new_table = pmm_alloc_page();

        if (!new_table)
            return -1;

        page_table = (uint32_t *)new_table;

        memset(page_table, 0, PAGE_SIZE);

        page_directory[directory_index] =
            ((uint32_t)new_table & PAGE_FRAME_MASK) |
            PAGE_PRESENT |
            PAGE_WRITABLE |
            (flags & PAGE_USER);
    }
    else {
        page_table =
            (uint32_t *)(page_directory[directory_index] &
                         PAGE_FRAME_MASK);
    }

    /*
     * Create the actual page mapping.
     */
    page_table[table_index] =
        physical_addr |
        (flags & 0xFFFU);

    /*
     * Flush this virtual address from the TLB.
     */
    asm volatile(
        "invlpg (%0)"
        :
        : "r"(virtual_addr)
        : "memory"
    );

    return 0;
}


/*
 * Unmap one virtual page.
 */

int unmap_page(uint32_t virtual_addr)
{
    uint32_t *page;

    virtual_addr &= PAGE_FRAME_MASK;

    page = get_page(virtual_addr);

    if (!page)
        return -1;

    *page = 0;

    /*
     * Flush the stale mapping from the TLB.
     */
    asm volatile(
        "invlpg (%0)"
        :
        : "r"(virtual_addr)
        : "memory"
    );

    return 0;
}


/*
 * Initialize paging.
 *
 * Identity maps the first 4 MiB.
 */

void init_paging(void)
{
    uint32_t i;

    /*
     * IMPORTANT:
     *
     * page_directory is exactly one 4 KiB page.
     * Do NOT clear 1024 pages here.
     */
    memset(page_directory, 0, PAGE_SIZE);

    memset(first_page_table, 0, PAGE_SIZE);

    /*
     * Identity-map the first 4 MiB:
     *
     * virtual 0x00000000 -> physical 0x00000000
     * virtual 0x00001000 -> physical 0x00001000
     * ...
     * virtual 0x003FF000 -> physical 0x003FF000
     */
    for (i = 0; i < PAGE_ENTRIES; i++) {
        first_page_table[i] =
            (i * 0x1000U) |
            PAGE_PRESENT |
            PAGE_WRITABLE;
    }

    /*
     * Page directory entry 0 points to the first
     * page table.
     */
    page_directory[0] =
        ((uint32_t)first_page_table & PAGE_FRAME_MASK) |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    /*
     * Load page directory into CR3.
     */
    asm volatile(
        "mov %0, %%cr3"
        :
        : "r"(page_directory)
        : "memory"
    );

    /*
     * Enable paging through CR0.PG.
     */
    {
        uint32_t cr0;

        asm volatile(
            "mov %%cr0, %0"
            : "=r"(cr0)
        );

        cr0 |= 0x80000000U;

        asm volatile(
            "mov %0, %%cr0"
            :
            : "r"(cr0)
            : "memory"
        );
    }
}