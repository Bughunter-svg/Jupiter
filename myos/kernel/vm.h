#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>

#define VM_PAGE_SIZE 4096U

/*
 * Virtual address range managed by the VMM.
 *
 * 0x01000000 = 16 MiB
 * 0x02000000 = 32 MiB
 */
#define VM_START 0x01000000U
#define VM_END   0x02000000U

#define VM_PAGE_COUNT ((VM_END - VM_START) / VM_PAGE_SIZE)

/*
 * Initialize the virtual memory manager.
 */
void vm_init(void);

/*
 * Allocate one virtual page.
 *
 * Returns the virtual address on success.
 * Returns NULL on failure.
 */
void *vm_alloc_page(void);

/*
 * Free one VMM-managed virtual page.
 *
 * Returns 0 on success.
 * Returns a negative value on failure.
 */
int vm_free_page(void *virtual_address);

/*
 * Return number of currently allocated VMM pages.
 */
size_t vm_get_used_pages(void);

/*
 * Return number of currently free VMM pages.
 */
size_t vm_get_free_pages(void);

/*
 * Print VMM statistics.
 */
void vm_print_stats(void);

#endif