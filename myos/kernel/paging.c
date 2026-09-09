#include "paging.h"
#include "memory.h"
#include "screen.h"
#include <stdint.h>

#define PAGE_PRESENT  0x001
#define PAGE_WRITABLE 0x002

static uint32_t page_directory[1024]
    __attribute__((aligned(4096)));

static uint32_t first_page_table[1024]
    __attribute__((aligned(4096)));

static void load_page_directory(uint32_t address) {
    asm volatile(
        "mov %0, %%cr3"
        :
        : "r"(address)
        : "memory"
    );
}

static void enable_paging(void) {
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

int paging_is_enabled(void) {
    uint32_t cr0;

    asm volatile(
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    return (cr0 & 0x80000000U) != 0;
}

void init_paging(void) {
    print("Initializing paging...\n");

    /*
     * Clear the page directory.
     */
    for (int i = 0; i < 1024; i++)
        page_directory[i] = 0;

    /*
     * Identity-map the first 4 MiB.
     *
     * Virtual address == physical address.
     */
    for (int i = 0; i < 1024; i++) {
        first_page_table[i] =
            ((uint32_t)i * PAGE_SIZE) |
            PAGE_PRESENT |
            PAGE_WRITABLE;
    }

    /*
     * Page directory entry 0 points to the first
     * page table at its physical address.
     */
    page_directory[0] =
        ((uint32_t)first_page_table) |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    /*
     * Our kernel is currently identity-mapped,
     * so the physical address is also the address
     * we can use for CR3.
     */
    load_page_directory((uint32_t)page_directory);

    enable_paging();

    if (paging_is_enabled())
        print("Paging enabled.\n");
    else
        print("Paging FAILED.\n");
}
