#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_EXIT   1
#define SYS_WRITE  2
#define SYS_GETPID 3
#define SYS_TICKS  4
#define SYS_READ   5

#define SYSCALL_ERROR 0xFFFFFFFFU

void syscall_init(void);
uint32_t syscall_handler(uint32_t number, uint32_t arg1, uint32_t arg2, uint32_t arg3);
int syscall_exit_pending(void);

#endif
