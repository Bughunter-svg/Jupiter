#include "memory.h"
#include "screen.h"

typedef struct block_header {
    size_t size;
    int free;
    struct block_header *next;
} block_header_t;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t framebuffer_reserved;
} __attribute__((packed)) MultibootInfo;

typedef struct {
    uint32_t size;
    uint64_t base;
    uint64_t length;
    uint32_t type;
} __attribute__((packed)) MultibootMemoryEntry;

extern char kernel_end;

static void print_hex64(uint64_t value);

static block_header_t *free_list = (block_header_t *)0;
static uint8_t *heap_base = (uint8_t *)0;
static size_t heap_used = 0;
static size_t total_ram = 0;
static uint8_t *heap_start = (uint8_t *)0;
static uint8_t *heap_end = (uint8_t *)0;

static size_t align_size(size_t size) {
    return (size + 7) & ~7U;
}

static size_t header_size(void) {
    return align_size(sizeof(block_header_t));
}

void mem_init(void) {
    heap_base = (uint8_t *)align_size((size_t)&kernel_end);
    heap_start = heap_base;
    heap_end = heap_start;
    heap_used = 0;
    free_list = (block_header_t *)0;

    print("Memory Manager Initialized\n");
}

void mem_set_total(size_t total) {
    total_ram = total;

    if (total_ram < MIN_HEAP_SIZE)
        total_ram = MIN_HEAP_SIZE;

    heap_end = (uint8_t *)total_ram;

    if (heap_start >= heap_end)
        heap_start = heap_end;
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

    if ((uint8_t *)ptr < heap_base + hdr)
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

#define MULTIBOOT_FLAG_MEM_MAP (1 << 6)

static MemoryRegion memory_map[MAX_MEMORY_REGIONS];
static size_t memory_region_count = 0;
static size_t usable_ram = 0;

void mem_detect_multiboot(unsigned int *mb_info) {
    if (!mb_info)
        return;

    MultibootInfo *info = (MultibootInfo *)mb_info;
    unsigned int flags = info->flags;

    memory_region_count = 0;
    usable_ram = 0;

    if (flags & MULTIBOOT_FLAG_MEM_MAP) {
        uint32_t mmap_length = info->mmap_length;
        uint32_t mmap_addr = info->mmap_addr;
        uint32_t offset = 0;

        while (offset < mmap_length &&
               memory_region_count < MAX_MEMORY_REGIONS) {

            MultibootMemoryEntry *entry =
                (MultibootMemoryEntry *)(mmap_addr + offset);

            memory_map[memory_region_count].base = entry->base;
            memory_map[memory_region_count].length = entry->length;
            memory_map[memory_region_count].type = entry->type;

            if (entry->type == 1)
                usable_ram += (size_t)entry->length;

            memory_region_count++;

            offset += entry->size + sizeof(entry->size);
        }

        mem_set_total(usable_ram);
        return;
    }

    if (flags & (1 << 0)) {
        unsigned int mem_upper = info->mem_upper;
        size_t total = ((size_t)mem_upper + 1024) * 1024;

        mem_set_total(total);
        return;
    }

    mem_set_total(0);
}

void mem_print_map(void) {
    print("\nJupiterOS Memory Map\n");
    print("====================\n");

    for (size_t i = 0; i < memory_region_count; i++) {
        print("Region ");
        print_int((int)i);
        print(": ");

        print("Base=");
        print_hex64(memory_map[i].base);

        print(" Length=");
        print_hex64(memory_map[i].length);

        print(" Type=");

        if (memory_map[i].type == 1)
            print("USABLE");
        else
            print("RESERVED");

        print("\n");
    }

    print("Usable RAM: ");
    print_int((int)(usable_ram / (1024 * 1024)));
    print(" MB\n");
}

size_t mem_get_usable_ram(void) {
    return usable_ram;
}

#define MAX_PHYSICAL_PAGES 1048576U

static uint8_t pmm_bitmap[BITMAP_SIZE];

static size_t pmm_total_pages = 0;
static size_t pmm_free_pages = 0;

static void pmm_set_used(size_t page) {
    pmm_bitmap[page / 8] |= (uint8_t)(1U << (page % 8));
}

static void pmm_set_free(size_t page) {
    pmm_bitmap[page / 8] &= (uint8_t)~(1U << (page % 8));
}

static int pmm_is_free(size_t page) {
    return !(pmm_bitmap[page / 8] & (1U << (page % 8)));
}

static void print_hex64(uint64_t value) {
    print_hex((uint32_t)(value >> 32));
    print_hex((uint32_t)value);
}

static void pmm_reserve_region(uint64_t base, uint64_t length) {
    uint64_t start = base & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t end = (base + length + PAGE_SIZE - 1) &
                   ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        size_t page = (size_t)(addr / PAGE_SIZE);

        if (page >= MAX_PHYSICAL_PAGES)
            break;

        if (pmm_is_free(page)) {
            pmm_set_used(page);

            if (pmm_free_pages > 0)
                pmm_free_pages--;
        }
    }
}

static void pmm_mark_usable_region(uint64_t base, uint64_t length) {
    uint64_t start = (base + PAGE_SIZE - 1) &
                     ~(uint64_t)(PAGE_SIZE - 1);

    uint64_t end = (base + length) &
                   ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        size_t page = (size_t)(addr / PAGE_SIZE);

        if (page >= MAX_PHYSICAL_PAGES)
            break;

        if (!pmm_is_free(page)) {
            pmm_set_free(page);
            pmm_free_pages++;
        }
    }
}

static void pmm_reserve_string(uint32_t address) {
    if (!address)
        return;

    uint8_t *str = (uint8_t *)address;
    size_t length = 0;

    while (str[length] != '\0')
        length++;

    pmm_reserve_region(address, length + 1);
}

void pmm_init(unsigned int *mb_info) {
    uint64_t highest_usable_address = 0;

    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));

    pmm_total_pages = 0;
    pmm_free_pages = 0;

    if (!mb_info)
        return;

    MultibootInfo *info = (MultibootInfo *)mb_info;
    unsigned int flags = info->flags;

    if (!(flags & MULTIBOOT_FLAG_MEM_MAP))
        return;

    uint32_t mmap_length = info->mmap_length;
    uint32_t mmap_addr = info->mmap_addr;
    uint32_t offset = 0;

    while (offset < mmap_length) {
        MultibootMemoryEntry *entry =
            (MultibootMemoryEntry *)(mmap_addr + offset);

        uint64_t end = entry->base + entry->length;

        if (entry->type == 1 &&
            end > highest_usable_address) {
            highest_usable_address = end;
        }

        if (entry->type == 1) {
            pmm_mark_usable_region(
                entry->base,
                entry->length
            );
        }

        offset += entry->size + sizeof(entry->size);
    }

    pmm_reserve_region(0, 0x100000);

    pmm_reserve_region(
        0x100000,
        (uint64_t)(uint32_t)&kernel_end - 0x100000
    );

    pmm_reserve_region(
        (uint32_t)mb_info,
        sizeof(MultibootInfo)
    );

    pmm_reserve_region(
        info->mmap_addr,
        info->mmap_length
    );

    if (flags & (1 << 2))
        pmm_reserve_string(info->cmdline);

    if (flags & (1 << 9))
        pmm_reserve_string(info->boot_loader_name);

    if (flags & (1 << 11)) {
        if (info->vbe_control_info)
            pmm_reserve_region(
                info->vbe_control_info,
                512
            );

        if (info->vbe_mode_info)
            pmm_reserve_region(
                info->vbe_mode_info,
                256
            );
    }

    if (flags & (1 << 12)) {
        uint64_t framebuffer_size =
            (uint64_t)info->framebuffer_pitch *
            (uint64_t)info->framebuffer_height;

        if (info->framebuffer_addr &&
            framebuffer_size) {
            pmm_reserve_region(
                info->framebuffer_addr,
                framebuffer_size
            );
        }
    }

    pmm_total_pages =
        (size_t)((highest_usable_address + PAGE_SIZE - 1) /
                 PAGE_SIZE);

    if (pmm_total_pages > MAX_PHYSICAL_PAGES)
        pmm_total_pages = MAX_PHYSICAL_PAGES;

    print("Physical Memory Manager initialized.\n");

    print("Total pages: ");
    print_int((int)pmm_total_pages);
    print("\n");

    print("Free pages: ");
    print_int((int)pmm_free_pages);
    print("\n");
}

void *pmm_alloc_page(void) {
    for (size_t page = 0; page < pmm_total_pages; page++) {
        if (pmm_is_free(page)) {
            pmm_set_used(page);

            if (pmm_free_pages > 0)
                pmm_free_pages--;

            return (void *)(page * PAGE_SIZE);
        }
    }

    print("PMM: OUT OF PHYSICAL MEMORY\n");
    return (void *)0;
}

void pmm_free_page(void *page_addr) {
    uint32_t addr = (uint32_t)page_addr;

    if (addr % PAGE_SIZE != 0)
        return;

    size_t page = addr / PAGE_SIZE;

    if (page >= pmm_total_pages)
        return;

    if (pmm_is_free(page))
        return;

    if (addr < 0x100000)
        return;

    if (addr < (uint32_t)&kernel_end)
        return;

    pmm_set_free(page);
    pmm_free_pages++;
}

size_t pmm_get_total_pages(void) {
    return pmm_total_pages;
}

size_t pmm_get_free_pages(void) {
    return pmm_free_pages;
}

void pmm_print_stats(void) {
    print("\nPhysical Memory Manager\n");
    print("=======================\n");

    print("Total pages: ");
    print_int((int)pmm_total_pages);
    print("\n");

    print("Free pages:  ");
    print_int((int)pmm_free_pages);
    print("\n");

    print("Used pages:  ");
    print_int((int)(pmm_total_pages - pmm_free_pages));
    print("\n");

    print("Page size:   ");
    print_int(PAGE_SIZE);
    print(" bytes\n");

    print("Free RAM:    ");
    print_int((int)((pmm_free_pages * PAGE_SIZE) /
                   (1024 * 1024)));
    print(" MB\n");
}