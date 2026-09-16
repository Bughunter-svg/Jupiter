#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void init_gdt(void);
void gdt_set_tss_entry(uint32_t base, uint32_t limit);

#endif