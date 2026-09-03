#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_START 0x100000U
#define HEAP_SIZE  0x400000U
#define HEAP_END   (HEAP_START + HEAP_SIZE)

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

#endif
