#include "timer.h"
#include "ports.h"
#include "screen.h"

#define PIT_FREQUENCY 1193180U

volatile unsigned long ticks = 0;

void timer_handler() {
    ticks++;
}

void init_timer() {
    unsigned int divisor = PIT_FREQUENCY / 100;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    print("Timer initialized at 100Hz\n");
}

void sleep(int ms) {
    unsigned long start = ticks;
    while (ticks < start + ms) {
        asm volatile ("nop");
    }
}

unsigned long get_ticks() {
    return ticks;
}