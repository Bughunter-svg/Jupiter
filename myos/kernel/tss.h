#ifndef TSS_H
#define TSS_H

#include <stdint.h>

void tss_prepare(void);
void init_tss(void);
void tss_set_kernel_stack(uint32_t stack_top);

#endif