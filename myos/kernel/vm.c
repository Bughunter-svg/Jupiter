#include "vm.h"
#include "paging.h"
#include "memory.h"
#include "screen.h"
#include <stdint.h>

#define VM_BITMAP_SIZE ((VM_PAGE_COUNT + 7U) / 8U)

#define KVMALLOC_MAGIC 0x4A564D41U

typedef struct {
    uint32_t magic;
    uint32_t page_count;
} kvmalloc_header_t;

static uint8_t vm_bitmap[VM_BITMAP_SIZE];
static size_t vm_used_pages = 0;


/* ---------------------------------------------------------
 * Bitmap helpers
 * --------------------------------------------------------- */

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


/* ---------------------------------------------------------
 * Initialization
 * --------------------------------------------------------- */

void vm_init(void)
{
    size_t i;

    for (i = 0; i < VM_BITMAP_SIZE; i++)
        vm_bitmap[i] = 0;

    vm_used_pages = 0;

    print("Virtual Memory Manager initialized.\n");
}


/* ---------------------------------------------------------
 * Single-page allocation
 * --------------------------------------------------------- */

void *vm_alloc_page(void)
{
    size_t page;
    uint32_t virtual_address;
    void *physical_page;

    for (page = 0; page < VM_PAGE_COUNT; page++) {

        if (vm_is_used(page))
            continue;

        virtual_address =
            VM_START + (uint32_t)(page * VM_PAGE_SIZE);

        physical_page = pmm_alloc_page();

        if (!physical_page)
            return (void *)0;

        if (map_page(
                virtual_address,
                (uint32_t)physical_page,
                PAGE_PRESENT | PAGE_WRITABLE) < 0) {

            pmm_free_page(physical_page);
            return (void *)0;
        }

        vm_set_used(page);
        vm_used_pages++;

        return (void *)virtual_address;
    }

    print("VMM: OUT OF VIRTUAL ADDRESS SPACE\n");

    return (void *)0;
}


/* ---------------------------------------------------------
 * Single-page free
 * --------------------------------------------------------- */

int vm_free_page(void *virtual_address)
{
    uint32_t address;
    size_t page;
    uint32_t *page_entry;
    uint32_t physical_address;

    if (!virtual_address)
        return -1;

    address = (uint32_t)virtual_address;

    if (address & (VM_PAGE_SIZE - 1U))
        return -2;

    if (address < VM_START || address >= VM_END)
        return -3;

    page =
        (size_t)((address - VM_START) / VM_PAGE_SIZE);

    if (!vm_is_used(page))
        return -4;

    page_entry = get_page(address);

    if (!page_entry ||
        !(*page_entry & PAGE_PRESENT)) {

        return -5;
    }

    physical_address =
        *page_entry & 0xFFFFF000U;

    if (unmap_page(address) < 0)
        return -6;

    pmm_free_page((void *)physical_address);

    vm_set_free(page);

    if (vm_used_pages > 0)
        vm_used_pages--;

    return 0;
}


/* ---------------------------------------------------------
 * Multi-page allocation
 *
 * Finds COUNT consecutive free virtual pages.
 * Each page gets its own physical frame.
 * --------------------------------------------------------- */

void *vm_alloc_pages(size_t count)
{
    size_t start;
    size_t i;
    int found;
    uint32_t virtual_address;
    void *physical_page;

    if (count == 0)
        return (void *)0;

    if (count > VM_PAGE_COUNT)
        return (void *)0;

    /*
     * Search for a contiguous virtual range.
     */
    for (start = 0;
         start + count <= VM_PAGE_COUNT;
         start++) {

        found = 1;

        for (i = 0; i < count; i++) {

            if (vm_is_used(start + i)) {
                found = 0;
                break;
            }
        }

        if (found)
            break;
    }

    if (!found) {
        print("VMM: NO CONTIGUOUS VIRTUAL RANGE\n");
        return (void *)0;
    }

    /*
     * Map every page in the range.
     */
    for (i = 0; i < count; i++) {

        size_t page = start + i;

        virtual_address =
            VM_START +
            (uint32_t)(page * VM_PAGE_SIZE);

        physical_page = pmm_alloc_page();

        if (!physical_page) {

            /*
             * Roll back everything already mapped.
             */
            while (i > 0) {
                i--;

                page = start + i;

                virtual_address =
                    VM_START +
                    (uint32_t)(page * VM_PAGE_SIZE);

                {
                    uint32_t *entry =
                        get_page(virtual_address);

                    if (entry &&
                        (*entry & PAGE_PRESENT)) {

                        uint32_t physical_address =
                            *entry & 0xFFFFF000U;

                        unmap_page(virtual_address);

                        pmm_free_page(
                            (void *)physical_address);
                    }

                    vm_set_free(page);
                }
            }

            return (void *)0;
        }

        if (map_page(
                virtual_address,
                (uint32_t)physical_page,
                PAGE_PRESENT | PAGE_WRITABLE) < 0) {

            pmm_free_page(physical_page);

            /*
             * Roll back previously mapped pages.
             */
            while (i > 0) {
                i--;

                page = start + i;

                virtual_address =
                    VM_START +
                    (uint32_t)(page * VM_PAGE_SIZE);

                {
                    uint32_t *entry =
                        get_page(virtual_address);

                    if (entry &&
                        (*entry & PAGE_PRESENT)) {

                        uint32_t physical_address =
                            *entry & 0xFFFFF000U;

                        unmap_page(virtual_address);

                        pmm_free_page(
                            (void *)physical_address);
                    }

                    vm_set_free(page);
                }
            }

            return (void *)0;
        }

        vm_set_used(page);
        vm_used_pages++;
    }

    return (void *)(
        VM_START +
        (uint32_t)(start * VM_PAGE_SIZE)
    );
}


/* ---------------------------------------------------------
 * Multi-page free
 * --------------------------------------------------------- */

int vm_free_pages(void *virtual_address, size_t count)
{
    uint32_t address;
    size_t start;
    size_t i;

    if (!virtual_address)
        return -1;

    if (count == 0)
        return -2;

    address = (uint32_t)virtual_address;

    if (address & (VM_PAGE_SIZE - 1U))
        return -3;

    if (address < VM_START || address >= VM_END)
        return -4;

    start =
        (size_t)((address - VM_START) / VM_PAGE_SIZE);

    if (count > VM_PAGE_COUNT - start)
        return -5;

    /*
     * Verify the entire range first.
     * This prevents a partially freed allocation.
     */
    for (i = 0; i < count; i++) {

        if (!vm_is_used(start + i))
            return -6;

        {
            uint32_t page_address =
                VM_START +
                (uint32_t)((start + i) * VM_PAGE_SIZE);

            uint32_t *entry =
                get_page(page_address);

            if (!entry ||
                !(*entry & PAGE_PRESENT)) {

                return -7;
            }
        }
    }

    /*
     * Everything is valid.
     * Now free the entire range.
     */
    for (i = 0; i < count; i++) {

        uint32_t page_address =
            VM_START +
            (uint32_t)((start + i) * VM_PAGE_SIZE);

        uint32_t *entry =
            get_page(page_address);

        uint32_t physical_address =
            *entry & 0xFFFFF000U;

        unmap_page(page_address);

        pmm_free_page(
            (void *)physical_address);

        vm_set_free(start + i);

        if (vm_used_pages > 0)
            vm_used_pages--;
    }

    return 0;
}


void *vm_alloc_guarded_pages(size_t count)
{
    size_t start;
    size_t i;
    size_t total;
    uint32_t virtual_address;
    void *physical_page;

    if (count == 0 || count > VM_PAGE_COUNT - 2U)
        return (void *)0;

    total = count + 2U;

    for (start = 0; start + total <= VM_PAGE_COUNT; start++) {
        int found = 1;

        for (i = 0; i < total; i++) {
            if (vm_is_used(start + i)) {
                found = 0;
                break;
            }
        }

        if (found)
            break;
    }

    if (start + total > VM_PAGE_COUNT)
        return (void *)0;

    vm_set_used(start);
    vm_set_used(start + count + 1U);

    for (i = 0; i < count; i++) {
        size_t page = start + 1U + i;

        virtual_address =
            VM_START +
            (uint32_t)(page * VM_PAGE_SIZE);

        physical_page = pmm_alloc_page();

        if (!physical_page) {
            while (i > 0) {
                uint32_t rollback_address;
                uint32_t *entry;
                uint32_t physical_address;

                i--;
                page = start + 1U + i;

                rollback_address =
                    VM_START +
                    (uint32_t)(page * VM_PAGE_SIZE);

                entry = get_page(rollback_address);

                if (entry && (*entry & PAGE_PRESENT)) {
                    physical_address =
                        *entry & 0xFFFFF000U;

                    unmap_page(rollback_address);
                    pmm_free_page((void *)physical_address);
                }

                vm_set_free(page);
                if (vm_used_pages > 0)
                    vm_used_pages--;
            }

            vm_set_free(start);
            vm_set_free(start + count + 1U);
            return (void *)0;
        }

        if (map_page(
                virtual_address,
                (uint32_t)physical_page,
                PAGE_PRESENT | PAGE_WRITABLE) < 0) {

            pmm_free_page(physical_page);

            while (i > 0) {
                uint32_t rollback_address;
                uint32_t *entry;
                uint32_t physical_address;

                i--;
                page = start + 1U + i;

                rollback_address =
                    VM_START +
                    (uint32_t)(page * VM_PAGE_SIZE);

                entry = get_page(rollback_address);

                if (entry && (*entry & PAGE_PRESENT)) {
                    physical_address =
                        *entry & 0xFFFFF000U;

                    unmap_page(rollback_address);
                    pmm_free_page((void *)physical_address);
                }

                vm_set_free(page);
                if (vm_used_pages > 0)
                    vm_used_pages--;
            }

            vm_set_free(start);
            vm_set_free(start + count + 1U);
            return (void *)0;
        }

        vm_set_used(page);
        vm_used_pages++;
    }

    return (void *)(
        VM_START +
        (uint32_t)((start + 1U) * VM_PAGE_SIZE)
    );
}

int vm_free_guarded_pages(void *virtual_address, size_t count)
{
    uint32_t address;
    size_t start;
    size_t i;

    if (!virtual_address || count == 0)
        return -1;

    address = (uint32_t)virtual_address;

    if (address & (VM_PAGE_SIZE - 1U))
        return -2;

    if (address < VM_START + VM_PAGE_SIZE ||
        address >= VM_END - VM_PAGE_SIZE)
        return -3;

    start =
        (size_t)((address - VM_START) / VM_PAGE_SIZE) - 1U;

    if (count > VM_PAGE_COUNT - start - 2U)
        return -4;

    if (!vm_is_used(start) ||
        !vm_is_used(start + count + 1U))
        return -5;

    for (i = 0; i < count; i++) {
        size_t page = start + 1U + i;
        uint32_t page_address =
            VM_START +
            (uint32_t)(page * VM_PAGE_SIZE);
        uint32_t *entry =
            get_page(page_address);

        if (!vm_is_used(page) ||
            !entry ||
            !(*entry & PAGE_PRESENT))
            return -6;
    }

    for (i = 0; i < count; i++) {
        size_t page = start + 1U + i;
        uint32_t page_address =
            VM_START +
            (uint32_t)(page * VM_PAGE_SIZE);
        uint32_t *entry =
            get_page(page_address);
        uint32_t physical_address =
            *entry & 0xFFFFF000U;

        unmap_page(page_address);
        pmm_free_page((void *)physical_address);
        vm_set_free(page);

        if (vm_used_pages > 0)
            vm_used_pages--;
    }

    vm_set_free(start);
    vm_set_free(start + count + 1U);

    return 0;
}


/* ---------------------------------------------------------
 * VMM-backed kernel allocation
 *
 * Allocates enough complete virtual pages to hold:
 *
 *     allocation header + requested bytes
 *
 * The header is stored at the beginning of the allocation.
 * The pointer returned to the caller points immediately
 * after the header.
 * --------------------------------------------------------- */

void *kvmalloc(size_t size)
{
    size_t total_size;
    size_t page_count;
    kvmalloc_header_t *header;
    void *base;

    if (size == 0)
        return (void *)0;

    /*
     * Protect the size calculation from integer overflow.
     */
    if (size >
        (size_t)-1 -
        sizeof(kvmalloc_header_t) -
        (VM_PAGE_SIZE - 1U)) {

        print("kvmalloc: SIZE OVERFLOW\n");
        return (void *)0;
    }

    total_size =
        size +
        sizeof(kvmalloc_header_t);

    page_count =
        (total_size + VM_PAGE_SIZE - 1U) /
        VM_PAGE_SIZE;

    if (page_count == 0 ||
        page_count > VM_PAGE_COUNT) {

        print("kvmalloc: REQUEST TOO LARGE\n");
        return (void *)0;
    }

    base = vm_alloc_pages(page_count);

    if (!base) {
        print("kvmalloc: OUT OF MEMORY\n");
        return (void *)0;
    }

    header = (kvmalloc_header_t *)base;

    header->magic = KVMALLOC_MAGIC;
    header->page_count = (uint32_t)page_count;

    return (void *)(
        (uint8_t *)base +
        sizeof(kvmalloc_header_t)
    );
}


/* ---------------------------------------------------------
 * VMM-backed kernel free
 * --------------------------------------------------------- */

void kvfree(void *ptr)
{
    kvmalloc_header_t *header;
    uint32_t page_count;
    void *base;

    if (!ptr)
        return;

    /*
     * The header is immediately before the returned pointer.
     */
    header =
        (kvmalloc_header_t *)(
            (uint8_t *)ptr -
            sizeof(kvmalloc_header_t)
        );

    if (header->magic != KVMALLOC_MAGIC) {
        print("kvfree: INVALID POINTER\n");
        return;
    }

    page_count = header->page_count;

    if (page_count == 0 ||
        page_count > VM_PAGE_COUNT) {

        print("kvfree: INVALID ALLOCATION\n");
        return;
    }

    base = (void *)header;

    /*
     * Invalidate the header before releasing the pages.
     * This also makes accidental repeated frees fail
     * instead of silently freeing the allocation twice.
     */
    header->magic = 0;
    header->page_count = 0;

    if (vm_free_pages(base, page_count) != 0) {
        print("kvfree: FAILED TO FREE ALLOCATION\n");
        return;
    }
}


/* ---------------------------------------------------------
 * Statistics
 * --------------------------------------------------------- */

size_t vm_get_used_pages(void)
{
    return vm_used_pages;
}

size_t vm_get_free_pages(void)
{
    return VM_PAGE_COUNT - vm_used_pages;
}


/* ---------------------------------------------------------
 * Statistics display
 * --------------------------------------------------------- */

void vm_print_stats(void)
{
    print("\nVirtual Memory Manager\n");
    print("======================\n");

    print("Virtual start: ");
    print_hex(VM_START);
    print("\n");

    print("Virtual end:   ");
    print_hex(VM_END);
    print("\n");

    print("Total pages:   ");
    print_int((int)VM_PAGE_COUNT);
    print("\n");

    print("Used pages:    ");
    print_int((int)vm_used_pages);
    print("\n");

    print("Free pages:    ");
    print_int((int)(VM_PAGE_COUNT - vm_used_pages));
    print("\n");
}