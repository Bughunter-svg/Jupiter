#include "paging.h"
#include "memory.h"
#include "screen.h"
#include <stdint.h>

#define PAGE_ENTRIES        1024U
#define PAGE_FRAME_MASK     0xFFFFF000U
#define RECURSIVE_INDEX     1023U
#define RECURSIVE_BASE      0xFFC00000U

#define PAGE_FAULT_PROTECTION    0x001U
#define PAGE_FAULT_WRITE         0x002U
#define PAGE_FAULT_USER          0x004U
#define PAGE_FAULT_RESERVED_BIT  0x008U
#define PAGE_FAULT_INSTRUCTION   0x010U

#define PAGE_FAULT_TEST_ADDRESS  0x00800000U
#define NULL_PAGE_ADDRESS        0x00000000U
#define READ_ONLY_TEST_ADDRESS  0x00C00000U

static uint32_t page_directory[PAGE_ENTRIES]
    __attribute__((aligned(4096)));

static uint32_t first_page_table[PAGE_ENTRIES]
    __attribute__((aligned(4096)));

static volatile int null_test_active = 0;
static volatile int null_test_faulted = 0;
static void *null_test_page = (void *)0;
static volatile int ro_test_active = 0;
static volatile int ro_test_faulted = 0;
static void *ro_test_page = (void *)0;

void page_fault_handler(uint32_t error_code)
{
    uint32_t fault_address;
    void *physical_page;

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

    if (null_test_active &&
        fault_address == NULL_PAGE_ADDRESS &&
        !(error_code & PAGE_FAULT_PROTECTION)) {

        null_test_faulted = 1;

        null_test_page = pmm_alloc_page();
        physical_page = null_test_page;

        if (physical_page) {
            if (map_page(
                    NULL_PAGE_ADDRESS,
                    (uint32_t)physical_page,
                    PAGE_PRESENT | PAGE_WRITABLE) == 0) {

                print("Null page temporarily mapped.\n");
                print("Page fault recovered.\n");
                return;
            }

            pmm_free_page(physical_page);
            null_test_page = (void *)0;
        }

        print("Null page test recovery FAILED.\n");
    }

    if (ro_test_active &&
        fault_address == READ_ONLY_TEST_ADDRESS &&
        (error_code & PAGE_FAULT_PROTECTION) &&
        (error_code & PAGE_FAULT_WRITE)) {

        ro_test_faulted = 1;

        {
            uint32_t *page = get_page(READ_ONLY_TEST_ADDRESS);

            if (page && (*page & PAGE_PRESENT)) {
                *page |= PAGE_WRITABLE;

                asm volatile(
                    "invlpg (%0)"
                    :
                    : "r"(READ_ONLY_TEST_ADDRESS)
                    : "memory"
                );

                print("Read-only violation detected.\n");
                print("Page temporarily made writable.\n");
                print("Page fault recovered.\n");
                return;
            }
        }

        print("Read-only page test recovery FAILED.\n");
    }

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
                return;
            }

            pmm_free_page(physical_page);
        }

        print("Page fault recovery FAILED.\n");
    }

    print("System halted.\n");

    for (;;) {
        asm volatile(
            "cli\n"
            "hlt"
        );
    }
}

int paging_is_enabled(void)
{
    uint32_t cr0;

    asm volatile(
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    return (cr0 & 0x80000000U) != 0;
}

uint32_t *get_page(uint32_t virtual_addr)
{
    uint32_t directory_index;
    uint32_t table_index;
    uint32_t *page_table;

    directory_index = virtual_addr >> 22;
    table_index = (virtual_addr >> 12) & 0x3FFU;

    if (!(page_directory[directory_index] & PAGE_PRESENT))
        return 0;

    page_table =
        (uint32_t *)(
            RECURSIVE_BASE +
            (directory_index * PAGE_SIZE)
        );

    return &page_table[table_index];
}

int map_page(uint32_t virtual_addr,
             uint32_t physical_addr,
             uint32_t flags)
{
    uint32_t directory_index;
    uint32_t table_index;
    uint32_t *page_table;
    void *new_table;

    virtual_addr &= PAGE_FRAME_MASK;
    physical_addr &= PAGE_FRAME_MASK;

    directory_index = virtual_addr >> 22;
    table_index = (virtual_addr >> 12) & 0x3FFU;

    if (!(page_directory[directory_index] & PAGE_PRESENT)) {
        new_table = pmm_alloc_page();

        if (!new_table)
            return -1;

        page_directory[directory_index] =
            ((uint32_t)new_table & PAGE_FRAME_MASK) |
            PAGE_PRESENT |
            PAGE_WRITABLE |
            (flags & PAGE_USER);

        page_table =
            (uint32_t *)(
                RECURSIVE_BASE +
                (directory_index * PAGE_SIZE)
            );

        memset(page_table, 0, PAGE_SIZE);
    } else {
        page_table =
            (uint32_t *)(
                RECURSIVE_BASE +
                (directory_index * PAGE_SIZE)
            );
    }

    page_table[table_index] =
        physical_addr |
        (flags & 0xFFFU);

    asm volatile(
        "invlpg (%0)"
        :
        : "r"(virtual_addr)
        : "memory"
    );

    return 0;
}

int unmap_page(uint32_t virtual_addr)
{
    uint32_t *page;

    virtual_addr &= PAGE_FRAME_MASK;

    page = get_page(virtual_addr);

    if (!page)
        return -1;

    *page = 0;

    asm volatile(
        "invlpg (%0)"
        :
        : "r"(virtual_addr)
        : "memory"
    );

    return 0;
}

int null_page_test(void)
{
    volatile uint32_t *null_ptr;
    uint32_t value;
    uint32_t *page;
    int result = 1;

    null_test_active = 1;
    null_test_faulted = 0;
    null_test_page = (void *)0;

    print("\nNull Page Protection Test\n");
    print("=========================\n");
    print("Accessing address: 0x00000000\n");

    null_ptr = (volatile uint32_t *)NULL_PAGE_ADDRESS;

    *null_ptr = 0x4E554C4CU;

    value = *null_ptr;

    if (!null_test_faulted)
        result = 0;

    if (value != 0x4E554C4CU)
        result = 0;

    if (unmap_page(NULL_PAGE_ADDRESS) != 0)
        result = 0;

    page = get_page(NULL_PAGE_ADDRESS);

    if (page && (*page & PAGE_PRESENT))
        result = 0;

    if (null_test_page)
        pmm_free_page(null_test_page);

    null_test_page = (void *)0;
    null_test_active = 0;

    if (result)
        print("Null page protection: PASS\n");
    else
        print("Null page protection: FAIL\n");

    return result;
}


int read_only_page_test(void)
{
    volatile uint32_t *test_ptr;
    uint32_t *page;
    uint32_t value;
    int result = 1;

    ro_test_active = 1;
    ro_test_faulted = 0;
    ro_test_page = pmm_alloc_page();

    print("\nRead-Only Page Protection Test\n");
    print("==============================\n");

    if (!ro_test_page) {
        print("Page allocation: FAIL\n");
        ro_test_active = 0;
        return 0;
    }

    if (map_page(
            READ_ONLY_TEST_ADDRESS,
            (uint32_t)ro_test_page,
            PAGE_PRESENT | PAGE_WRITABLE) != 0) {

        print("Page mapping: FAIL\n");
        pmm_free_page(ro_test_page);
        ro_test_page = (void *)0;
        ro_test_active = 0;
        return 0;
    }

    test_ptr = (volatile uint32_t *)READ_ONLY_TEST_ADDRESS;
    *test_ptr = 0x524F5445U;

    page = get_page(READ_ONLY_TEST_ADDRESS);

    if (!page) {
        result = 0;
    } else {
        *page &= ~PAGE_WRITABLE;

        asm volatile(
            "invlpg (%0)"
            :
            : "r"(READ_ONLY_TEST_ADDRESS)
            : "memory"
        );
    }

    if (result)
        print("Page allocation: PASS\n");
    else
        print("Page allocation: FAIL\n");

    if (result && page && !(*page & PAGE_WRITABLE))
        print("Read-only mapping: PASS\n");
    else {
        print("Read-only mapping: FAIL\n");
        result = 0;
    }

    if (result) {
        value = *test_ptr;

        if (value == 0x524F5445U)
            print("Read access: PASS\n");
        else {
            print("Read access: FAIL\n");
            result = 0;
        }
    }

    if (result)
        *test_ptr = 0x524F5432U;

    if (!ro_test_faulted)
        result = 0;

    value = *test_ptr;

    if (value == 0x524F5432U)
        print("Write after recovery: PASS\n");
    else
        result = 0;

    page = get_page(READ_ONLY_TEST_ADDRESS);

    if (page) {
        *page &= ~PAGE_WRITABLE;

        asm volatile(
            "invlpg (%0)"
            :
            : "r"(READ_ONLY_TEST_ADDRESS)
            : "memory"
        );
    } else {
        result = 0;
    }

    if (unmap_page(READ_ONLY_TEST_ADDRESS) != 0)
        result = 0;

    if (ro_test_page)
        pmm_free_page(ro_test_page);

    ro_test_page = (void *)0;
    ro_test_active = 0;

    page = get_page(READ_ONLY_TEST_ADDRESS);

    if (page && (*page & PAGE_PRESENT))
        result = 0;

    if (result)
        print("Read-only page protection: PASS\n");
    else
        print("Read-only page protection: FAIL\n");

    return result;
}

void init_paging(void)
{
    uint32_t i;

    memset(page_directory, 0, PAGE_SIZE);
    memset(first_page_table, 0, PAGE_SIZE);

    for (i = 1; i < PAGE_ENTRIES; i++) {
        first_page_table[i] =
            (i * 0x1000U) |
            PAGE_PRESENT |
            PAGE_WRITABLE;
    }

    page_directory[0] =
        ((uint32_t)first_page_table & PAGE_FRAME_MASK) |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    page_directory[RECURSIVE_INDEX] =
        ((uint32_t)page_directory & PAGE_FRAME_MASK) |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    asm volatile(
        "mov %0, %%cr3"
        :
        : "r"(page_directory)
        : "memory"
    );

    {
        uint32_t cr0;

        asm volatile(
            "mov %%cr0, %0"
            : "=r"(cr0)
        );

        cr0 |= 0x80000000U;
	cr0 |= 0x00010000U;

        asm volatile(
            "mov %0, %%cr0"
            :
            : "r"(cr0)
            : "memory"
        );
    }
}