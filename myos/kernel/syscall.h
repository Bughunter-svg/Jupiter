#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

void syscall_init(void);
int syscall_handler(uint32_t value);

#endif