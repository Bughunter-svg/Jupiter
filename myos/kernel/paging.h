#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT  0x001
#define PAGE_WRITABLE 0x002
#define PAGE_USER     0x004

void init_paging(void);
int paging_is_enabled(void);
int map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
int unmap_page(uint32_t virtual_addr);
uint32_t *get_page(uint32_t virtual_addr);
void page_fault_handler(uint32_t error_code);
int null_page_test(void);

#endif