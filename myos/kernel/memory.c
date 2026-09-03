#include "memory.h"
#include "screen.h"

/* ─── Bump-pointer heap ──────────────────────────────────────────── */
static uint8_t *heap_ptr = (uint8_t *)HEAP_START;
static size_t   heap_used = 0;
static size_t total_ram = 0;

void mem_init(void) {
    heap_ptr  = (uint8_t *)HEAP_START;
    heap_used = 0;
    print("Memory Manager Initialized\n");
}
void mem_set_total(size_t total) {
    total_ram = total;
}

/*
 * kmalloc – simple bump allocator with 8-byte alignment.
 * Returns NULL when heap is exhausted.
 */
void *kmalloc(size_t size) {
    if (size == 0) return (void *)0;

    /* Align size to 8 bytes */
    size_t aligned = (size + 7) & ~7U;

    if ((size_t)heap_ptr + aligned > HEAP_END) {
        print("kmalloc: OUT OF MEMORY\n");
        return (void *)0;
    }

    void *ptr  = (void *)heap_ptr;
    heap_ptr  += aligned;
    heap_used += aligned;
    return ptr;
}

/* Bump allocator – freeing individual blocks is not supported.
   kfree is a no-op kept for API compatibility.                   */
void kfree(void *ptr) {
    (void)ptr;
}

size_t mem_get_total(void) {
    return total_ram;
}

size_t mem_get_used(void) {
    return heap_used;
}

size_t mem_get_free(void) {
    if (total_ram > heap_used)
        return total_ram - heap_used;

    return 0;
}
void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t       *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return 0;
}
void mem_detect_multiboot(unsigned int *mb_info) {
    if (!mb_info)
        return;

    unsigned int flags = mb_info[0];

    if (flags & (1 << 0)) {
        unsigned int mem_lower = mb_info[1];
        unsigned int mem_upper = mb_info[2];

        size_t total = ((size_t)mem_upper + 1024) * 1024;
        mem_set_total(total);
        return;
    }

    mem_set_total(0);
}
