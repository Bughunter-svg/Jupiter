#include "vm.h"
#include "paging.h"
#include "memory.h"
#include "screen.h"
#include <stdint.h>

/*
 * One bit per virtual page.
 *
 * 4096 virtual pages = 512 bytes.
 *
 * 0 = free
 * 1 = allocated
 */
#define VM_BITMAP_SIZE ((VM_PAGE_COUNT + 7U) / 8U)

static uint8_t vm_bitmap[VM_BITMAP_SIZE];

static size_t vm_used_pages = 0;


/*
 * ------------------------------------------------------------
 * Bitmap helpers
 * ------------------------------------------------------------
 */

static int vm_is_used(size_t page)
{
    return (vm_bitmap[page / 8U] &
            (uint8_t)(1U << (page % 8U))) != 0;
}


static void vm_set_used(size_t page)
{
    vm_bitmap[page / 8U] |=
        (uint8_t)(1U << (page % 8U));
}


static void vm_set_free(size_t page)
{
    vm_bitmap[page / 8U] &=
        (uint8_t)~(1U << (page % 8U));
}


/*
 * ------------------------------------------------------------
 * Initialization
 * ------------------------------------------------------------
 */

void vm_init(void)
{
    size_t i;

    for (i = 0; i < VM_BITMAP_SIZE; i++)
        vm_bitmap[i] = 0;

    vm_used_pages = 0;

    print("Virtual Memory Manager initialized.\n");
}


/*
 * ------------------------------------------------------------
 * Allocate one virtual page
 * ------------------------------------------------------------
 */

void *vm_alloc_page(void)
{
    size_t page;
    void *physical_page;
    uint32_t virtual_address;

    /*
     * Find a free virtual page.
     */
    for (page = 0; page < VM_PAGE_COUNT; page++) {

        if (vm_is_used(page))
            continue;

        virtual_address =
            VM_START + (uint32_t)(page * VM_PAGE_SIZE);

        /*
         * Allocate a physical frame.
         */
        physical_page = pmm_alloc_page();

        if (!physical_page)
            return (void *)0;

        /*
         * Create the virtual -> physical mapping.
         */
        if (map_page(
                virtual_address,
                (uint32_t)physical_page,
                PAGE_PRESENT | PAGE_WRITABLE) < 0) {

            /*
             * Mapping failed, so give the physical
             * frame back to the PMM.
             */
            pmm_free_page(physical_page);

            return (void *)0;
        }

        /*
         * Mark virtual page as allocated.
         */
        vm_set_used(page);
        vm_used_pages++;

        return (void *)virtual_address;
    }

    print("VMM: OUT OF VIRTUAL ADDRESS SPACE\n");

    return (void *)0;
}


/*
 * ------------------------------------------------------------
 * Free one virtual page
 * ------------------------------------------------------------
 */

int vm_free_page(void *virtual_address)
{
    uint32_t address;
    size_t page;
    uint32_t *page_entry;
    uint32_t physical_address;

    if (!virtual_address)
        return -1;

    address = (uint32_t)virtual_address;

    /*
     * Must be page aligned.
     */
    if (address & (VM_PAGE_SIZE - 1U))
        return -2;

    /*
     * Must belong to our VMM address range.
     */
    if (address < VM_START || address >= VM_END)
        return -3;

    page =
        (size_t)((address - VM_START) / VM_PAGE_SIZE);

    /*
     * Must currently be allocated.
     */
    if (!vm_is_used(page))
        return -4;

    /*
     * Find the page-table entry.
     */
    page_entry = get_page(address);

    if (!page_entry ||
        !(*page_entry & PAGE_PRESENT)) {

        /*
         * Keep VMM bookkeeping consistent.
         */
        vm_set_free(page);

        if (vm_used_pages > 0)
            vm_used_pages--;

        return -5;
    }

    /*
     * Extract physical frame address.
     */
    physical_address =
        *page_entry & 0xFFFFF000U;

    /*
     * Remove the virtual mapping.
     */
    if (unmap_page(address) < 0)
        return -6;

    /*
     * Return the physical frame to the PMM.
     */
    pmm_free_page((void *)physical_address);

    /*
     * Mark virtual page free.
     */
    vm_set_free(page);

    if (vm_used_pages > 0)
        vm_used_pages--;

    return 0;
}


/*
 * ------------------------------------------------------------
 * Statistics
 * ------------------------------------------------------------
 */

size_t vm_get_used_pages(void)
{
    return vm_used_pages;
}


size_t vm_get_free_pages(void)
{
    return VM_PAGE_COUNT - vm_used_pages;
}


void vm_print_stats(void)
{
    print("\nVirtual Memory Manager\n");
    print("======================\n");

    print("Virtual range: ");
    print_hex(VM_START);
    print(" - ");
    print_hex(VM_END);
    print("\n");

    print("Total pages: ");
    print_int((int)VM_PAGE_COUNT);
    print("\n");

    print("Used pages:  ");
    print_int((int)vm_used_pages);
    print("\n");

    print("Free pages:  ");
    print_int((int)(VM_PAGE_COUNT - vm_used_pages));
    print("\n");
}