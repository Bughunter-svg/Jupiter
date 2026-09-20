#include "paging.h"
#include "vm.h"
#include "memory.h"
#include "screen.h"
#include <stdint.h>

#define PAGE_ENTRIES        1024U
#define PAGE_FRAME_MASK     0xFFFFF000U
#define RECURSIVE_INDEX     1023U
#define RECURSIVE_BASE      0xFFC00000U

/*
 * Self-map address: with PDE[RECURSIVE_INDEX] pointing at the directory
 * itself, accessing 0xFFC00000 + dirIdx*PAGE_SIZE gives you PAGE TABLE
 * #dirIdx's PTE array (used by get_page()/map_page() below - correct).
 *
 * To instead read/write the DIRECTORY's own PDE entries as raw data, you
 * have to go through the self-map TWICE: dirIdx=1023 (self) AND
 * tableIdx=1023 (self again). That address is RECURSIVE_BASE +
 * RECURSIVE_INDEX*PAGE_SIZE, i.e. 0xFFFFF000.
 *
 * paging_create_address_space() previously used RECURSIVE_BASE directly
 * for this, which actually landed on whatever PDE[0] points to (the
 * kernel's real, live first_page_table) instead of the new directory -
 * silently corrupting the running kernel's own identity map.
 */
#define PD_SELF_MAP         0xFFFFF000U

#define PAGE_FAULT_PROTECTION    0x001U
#define PAGE_FAULT_WRITE         0x002U
#define PAGE_FAULT_USER          0x004U
#define PAGE_FAULT_RESERVED_BIT  0x008U
#define PAGE_FAULT_INSTRUCTION   0x010U

#define PAGE_FAULT_TEST_ADDRESS  0x00800000U
#define NULL_PAGE_ADDRESS        0x00000000U
#define READ_ONLY_TEST_ADDRESS  0x00C00000U
#define GUARD_TEST_ADDRESS       0x00D00000U

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
static volatile int guard_test_active = 0;
static volatile int guard_test_faulted = 0;
static void *guard_test_page = (void *)0;
static volatile uint32_t guard_test_fault_address = 0;

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

    if (guard_test_active &&
        fault_address == guard_test_fault_address &&
        !(error_code & PAGE_FAULT_PROTECTION)) {

        guard_test_faulted = 1;

        guard_test_page = pmm_alloc_page();
        physical_page = guard_test_page;

        if (physical_page) {
            if (map_page(
                    guard_test_fault_address,
                    (uint32_t)physical_page,
                    PAGE_PRESENT | PAGE_WRITABLE) == 0) {

                print("Guard page violation detected.\n");
                print("Guard page temporarily mapped.\n");
                print("Page fault recovered.\n");
                return;
            }

            pmm_free_page(physical_page);
            guard_test_page = (void *)0;
        }

        print("Guard page test recovery FAILED.\n");
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
    uint32_t *current_page_directory;
    uint32_t *page_table;

    directory_index = virtual_addr >> 22;
    table_index = (virtual_addr >> 12) & 0x3FFU;
    current_page_directory = (uint32_t *)PD_SELF_MAP;

    if (!(current_page_directory[directory_index] & PAGE_PRESENT))
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
    uint32_t *current_page_directory;
    uint32_t *page_table;
    void *new_table;

    virtual_addr &= PAGE_FRAME_MASK;
    physical_addr &= PAGE_FRAME_MASK;

    directory_index = virtual_addr >> 22;
    table_index = (virtual_addr >> 12) & 0x3FFU;
    current_page_directory = (uint32_t *)PD_SELF_MAP;

    if (!(current_page_directory[directory_index] & PAGE_PRESENT)) {
        new_table = pmm_alloc_page();

        if (!new_table)
            return -1;

        current_page_directory[directory_index] =
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

        if (flags & PAGE_USER)
            current_page_directory[directory_index] |= PAGE_USER;
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


int guard_page_test(void)
{
    volatile uint32_t *guard_ptr;
    volatile uint32_t *valid_ptr;
    uint32_t *page;
    uint32_t value;
    uint32_t valid_address;
    uint32_t lower_guard_address;
    uint32_t upper_guard_address;
    int result = 1;

    guard_test_active = 1;
    guard_test_faulted = 0;
    guard_test_page = (void *)0;
    guard_test_fault_address = 0;

    print("\nGuard Page Protection Test\n");
    print("==========================\n");

    valid_address =
        (uint32_t)vm_alloc_guarded_pages(1);

    if (!valid_address) {
        print("Guarded allocation: FAIL\n");
        guard_test_active = 0;
        return 0;
    }

    lower_guard_address = valid_address - VM_PAGE_SIZE;
    upper_guard_address = valid_address + VM_PAGE_SIZE;
    guard_test_fault_address = lower_guard_address;

    print("Guarded allocation: PASS\n");

    page = get_page(lower_guard_address);

    if (page && !(*page & PAGE_PRESENT))
        print("Lower guard page unmapped: PASS\n");
    else {
        print("Lower guard page unmapped: FAIL\n");
        result = 0;
    }

    page = get_page(valid_address);

    if (page && (*page & PAGE_PRESENT))
        print("Allocated page mapped: PASS\n");
    else {
        print("Allocated page mapped: FAIL\n");
        result = 0;
    }

    page = get_page(upper_guard_address);

    if (page && !(*page & PAGE_PRESENT))
        print("Upper guard page unmapped: PASS\n");
    else {
        print("Upper guard page unmapped: FAIL\n");
        result = 0;
    }

    valid_ptr = (volatile uint32_t *)valid_address;

    if (result) {
        *valid_ptr = 0x47554152U;

        if (*valid_ptr == 0x47554152U)
            print("Valid access: PASS\n");
        else {
            print("Valid access: FAIL\n");
            result = 0;
        }
    }

    guard_ptr = (volatile uint32_t *)lower_guard_address;

    if (result)
        *guard_ptr = 0x47554152U;

    if (!guard_test_faulted) {
        print("Guard page fault: FAIL\n");
        result = 0;
    } else {
        print("Guard page fault: PASS\n");
    }

    value = *guard_ptr;

    if (value == 0x47554152U)
        print("Fault recovery: PASS\n");
    else {
        print("Fault recovery: FAIL\n");
        result = 0;
    }

    if (unmap_page(lower_guard_address) != 0)
        result = 0;

    if (guard_test_page)
        pmm_free_page(guard_test_page);

    guard_test_page = (void *)0;
    guard_test_fault_address = 0;
    guard_test_active = 0;

    if (vm_free_guarded_pages((void *)valid_address, 1) != 0)
        result = 0;

    page = get_page(lower_guard_address);

    if (page && (*page & PAGE_PRESENT))
        result = 0;

    page = get_page(valid_address);

    if (page && (*page & PAGE_PRESENT))
        result = 0;

    page = get_page(upper_guard_address);

    if (page && (*page & PAGE_PRESENT))
        result = 0;

    if (result)
        print("Guard page protection: PASS\n");
    else
        print("Guard page protection: FAIL\n");

    return result;
}

void general_protection_handler(uint32_t error_code)
{
    print("\nGENERAL PROTECTION FAULT\n");
    print("=========================\n");
    print("Error code: ");
    print_hex(error_code);
    print("\n");
    print("System halted.\n");

    for (;;) {
        asm volatile(
            "cli\n"
            "hlt"
        );
    }
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

uint32_t paging_get_current_cr3(void)
{
    uint32_t cr3;

    asm volatile (
        "mov %%cr3, %0"
        : "=r"(cr3)
    );

    return cr3;
}

uint32_t paging_create_address_space(void)
{
    void *new_pd_page;
    uint32_t new_pd_phys;
    uint32_t *new_pd;
    int i;

    print("AS1: Allocating page directory...\n");

    new_pd_page = pmm_alloc_page();

    if (!new_pd_page) {
        print("AS2: PMM allocation FAILED\n");
        return 0;
    }

    print("AS2: PD page allocated\n");

    new_pd_phys = (uint32_t)new_pd_page;

    print("AS3: PD physical address: ");
    print_hex(new_pd_phys);
    print("\n");

    new_pd = (uint32_t *)new_pd_phys;

    print("AS4: Initializing new page directory\n");

    for (i = 0; i < PAGE_ENTRIES; i++)
        new_pd[i] = 0;

    print("AS5: New PD cleared\n");

    for (i = 0; i < RECURSIVE_INDEX; i++)
        new_pd[i] = page_directory[i];

    print("AS6: Kernel mappings copied\n");

    new_pd[RECURSIVE_INDEX] =
        (new_pd_phys & PAGE_FRAME_MASK) |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    print("AS7: New recursive mapping installed\n");

    return new_pd_phys;
}

int paging_switch_address_space(uint32_t cr3)
{
    if (!cr3)
        return -1;

    asm volatile (
        "mov %0, %%cr3"
        :
        : "r"(cr3)
        : "memory"
    );

    return 0;
}