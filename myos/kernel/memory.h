#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_START 0x100000U
#define MIN_HEAP_SIZE 0x400000U

#define MAX_MEMORY_REGIONS 32
#define PAGE_SIZE 4096U
#define MAX_PHYSICAL_PAGES 1048576U
#define BITMAP_SIZE (MAX_PHYSICAL_PAGES / 8)

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} MemoryRegion;
void mem_init(void);
void mem_set_total(size_t total);
void mem_detect_multiboot(unsigned int *mb_info);

void *kmalloc(size_t size);
void kfree(void *ptr);

size_t mem_get_total(void);
size_t mem_get_used(void);
size_t mem_get_free(void);

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

/* Memory map */
void mem_print_map(void);
size_t mem_get_usable_ram(void);

/* Physical Memory Manager */
void pmm_init(unsigned int *mb_info);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
size_t pmm_get_total_pages(void);
size_t pmm_get_free_pages(void);
void pmm_print_stats(void);

#endif
