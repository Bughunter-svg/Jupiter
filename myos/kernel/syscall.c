#include "syscall.h"
#include "screen.h"

#define SYS_TEST 0x1337
#define SYS_EXIT 1

void syscall_init(void)
{
}

int syscall_handler(uint32_t value)
{
    print("DEBUG syscall value = ");
    print_hex(value);
    print("\n");

    if (value == SYS_TEST) {
        print("Ring 3 syscall reached Ring 0!\n");
        return 0;
    }

    if (value == SYS_EXIT) {
        print("Ring 3 process exiting...\n");
        return 1;
    }

    print("Unknown syscall\n");
    return 0;
}