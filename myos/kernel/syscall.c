#include "syscall.h"
#include "screen.h"

#define SYS_TEST 0x1337
#define SYS_EXIT 1

void syscall_init(void)
{
}

int syscall_handler(void)
{
    unsigned int value;

    asm volatile(
        "mov %%eax, %0"
        : "=r"(value)
    );

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