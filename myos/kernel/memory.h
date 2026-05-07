#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* ─── Heap layout ─────────────────────────────────────────────────── */
/* Kernel binary lives below 0x100000 (1 MB boundary).               */
/* We use 1 MB – 5 MB as a simple bump-pointer heap (4 MB).          */
#define HEAP_START   0x100000U   /* 1 MB                              */
#define HEAP_SIZE    0x400000U   /* 4 MB                              */
#define HEAP_END     (HEAP_START + HEAP_SIZE)

/* Total RAM we advertise to userspace commands (64 MB conservative)  */
#define TOTAL_RAM_BYTES  (64U * 1024U * 1024U)

/* ─── Public API ─────────────────────────────────────────────────── */
void  mem_init(void);
void *kmalloc(size_t size);
void  kfree(void *ptr);      /* bump allocator – kfree is a no-op    */

/* Live statistics (bytes) */
size_t mem_get_total(void);
size_t mem_get_used(void);
size_t mem_get_free(void);

/* memset / memcpy – needed by network and other modules */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);

#endif /* MEMORY_H */
