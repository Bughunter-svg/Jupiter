#include "memory.h"
#include "screen.h"

typedef struct block_header {
    size_t size;
    int free;
    struct block_header *next;
} block_header_t;

static block_header_t *free_list = (block_header_t *)0;
static size_t heap_used = 0;
static size_t total_ram = 0;
static uint8_t *heap_start = (uint8_t *)HEAP_START;
static uint8_t *heap_end = (uint8_t *)HEAP_START;

static size_t align_size(size_t size) {
    return (size + 7) & ~7U;
}

static size_t header_size(void) {
    return align_size(sizeof(block_header_t));
}

void mem_init(void) {
    heap_start = (uint8_t *)HEAP_START;
    heap_end = heap_start;
    heap_used = 0;
    free_list = (block_header_t *)0;
    print("Memory Manager Initialized\n");
}

void mem_set_total(size_t total) {
    total_ram = total;

    if (total_ram < MIN_HEAP_SIZE)
        total_ram = MIN_HEAP_SIZE;

    if (total_ram > 0x10000000U)
        total_ram = 0x10000000U;

    heap_end = (uint8_t *)total_ram;
}

void *kmalloc(size_t size) {
    if (size == 0)
        return (void *)0;

    size_t aligned = align_size(size);
    size_t hdr = header_size();

    block_header_t *current = free_list;
    block_header_t *previous = (block_header_t *)0;

    while (current) {
        if (current->free && current->size >= aligned) {
            if (previous)
                previous->next = current->next;
            else
                free_list = current->next;

            current->free = 0;
            current->next = (block_header_t *)0;

            heap_used += current->size;

            return (void *)((uint8_t *)current + hdr);
        }

        previous = current;
        current = current->next;
    }

    if (heap_start + hdr + aligned > heap_end) {
        print("kmalloc: OUT OF MEMORY\n");
        return (void *)0;
    }

    block_header_t *block = (block_header_t *)heap_start;

    block->size = aligned;
    block->free = 0;
    block->next = (block_header_t *)0;

    heap_start += hdr + aligned;
    heap_used += aligned;

    return (void *)((uint8_t *)block + hdr);
}

void kfree(void *ptr) {
    if (!ptr)
        return;

    size_t hdr = header_size();

    if ((uint8_t *)ptr < (uint8_t *)HEAP_START + hdr)
        return;

    if ((uint8_t *)ptr >= heap_start)
        return;

    block_header_t *block =
        (block_header_t *)((uint8_t *)ptr - hdr);

    if (block->free)
        return;

    block->free = 1;

    if (heap_used >= block->size)
        heap_used -= block->size;
    else
        heap_used = 0;

    block->next = free_list;
    free_list = block;
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

    while (n--)
        *p++ = (uint8_t)c;

    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    while (n--)
        *d++ = *s++;

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;

    while (n--) {
        if (*a != *b)
            return (int)*a - (int)*b;

        a++;
        b++;
    }

    return 0;
}

void mem_detect_multiboot(unsigned int *mb_info) {
    if (!mb_info)
        return;

    unsigned int flags = mb_info[0];

    if (flags & (1 << 0)) {
        unsigned int mem_upper = mb_info[2];
        size_t total = ((size_t)mem_upper + 1024) * 1024;

        mem_set_total(total);
        return;
    }

    mem_set_total(0);
}
