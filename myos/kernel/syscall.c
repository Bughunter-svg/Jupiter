#include "syscall.h"
#include "screen.h"
#include "paging.h"
#include "process.h"
#include "timer.h"
#include "keyboard.h"

#define WRITE_MAX 256U
#define PAGE_MASK 0xFFFFF000U
#define PAGE_STEP 0x1000U

extern uint32_t *get_page(uint32_t virtual_addr);

typedef uint32_t (*syscall_fn)(uint32_t, uint32_t, uint32_t);

static volatile int exit_flag = 0;

static int user_range_valid(uint32_t address, uint32_t length)
{
    uint32_t last;
    uint32_t page;
    uint32_t *entry;

    if (length == 0)
        return 1;

    if (address + length < address)
        return 0;

    last = (address + length - 1) & PAGE_MASK;

    for (page = address & PAGE_MASK; ; page += PAGE_STEP) {
        entry = get_page(page);

        if (!entry)
            return 0;

        if ((*entry & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER))
            return 0;

        if (page == last)
            break;
    }

    return 1;
}

static uint32_t sys_exit(uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)a1;
    (void)a2;
    (void)a3;

    exit_flag = 1;

    return 0;
}

static uint32_t sys_write(uint32_t buffer, uint32_t length, uint32_t a3)
{
    char text[WRITE_MAX + 1];
    const char *source;
    uint32_t i;

    (void)a3;

    if (length > WRITE_MAX)
        length = WRITE_MAX;

    if (!user_range_valid(buffer, length)) {
        print("[syscall] write rejected: bad user pointer\n");
        return SYSCALL_ERROR;
    }

    source = (const char *)buffer;

    for (i = 0; i < length; i++)
        text[i] = source[i];

    text[length] = '\0';

    print(text);

    return length;
}

static uint32_t sys_getpid(uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)a1;
    (void)a2;
    (void)a3;

    return (uint32_t)get_current_pid();
}

static uint32_t sys_ticks(uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)a1;
    (void)a2;
    (void)a3;

    return (uint32_t)get_ticks();
}

static uint32_t sys_read(uint32_t a1, uint32_t a2, uint32_t a3)
{
    int key;

    (void)a1;
    (void)a2;
    (void)a3;

    key = keyboard_poll();

    if (!key)
        return 0;

    return (uint32_t)(unsigned char)key;
}

static const syscall_fn syscall_table[] = {
    0,
    sys_exit,
    sys_write,
    sys_getpid,
    sys_ticks,
    sys_read
};

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_table[0]))

void syscall_init(void)
{
    exit_flag = 0;
}

uint32_t syscall_handler(uint32_t number, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (number >= SYSCALL_COUNT || !syscall_table[number]) {
        print("[syscall] unknown syscall\n");
        return SYSCALL_ERROR;
    }

    return syscall_table[number](arg1, arg2, arg3);
}

int syscall_exit_pending(void)
{
    int pending = exit_flag;

    exit_flag = 0;

    return pending;
}
