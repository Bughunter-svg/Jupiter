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

uint32_t paging_get_current_cr3(void);
uint32_t paging_create_address_space(void);
int paging_switch_address_space(uint32_t cr3);
int paging_destroy_address_space(uint32_t cr3);

void page_fault_handler(uint32_t error_code);

int null_page_test(void);
int read_only_page_test(void);
int guard_page_test(void);

#endif
