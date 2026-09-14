#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>

#define VM_PAGE_SIZE 4096U

#define VM_START 0x01000000U
#define VM_END   0x02000000U

#define VM_PAGE_COUNT ((VM_END - VM_START) / VM_PAGE_SIZE)

void vm_init(void);

void *vm_alloc_page(void);
int vm_free_page(void *virtual_address);

void *vm_alloc_pages(size_t count);
int vm_free_pages(void *virtual_address, size_t count);

void *kvmalloc(size_t size);
void kvfree(void *ptr);

size_t vm_get_used_pages(void);
size_t vm_get_free_pages(void);

void vm_print_stats(void);

#endif