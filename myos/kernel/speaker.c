#include "speaker.h"
#include "ports.h"
#include "timer.h"

#define PIT_BASE_FREQUENCY 1193180U

void speaker_on(unsigned int frequency)
{
    unsigned int divisor;
    unsigned char control;

    if (frequency < 20 || frequency > 20000)
        return;

    divisor = PIT_BASE_FREQUENCY / frequency;

    outb(0x43, 0xB6);
    outb(0x42, (unsigned char)(divisor & 0xFF));
    outb(0x42, (unsigned char)((divisor >> 8) & 0xFF));

    control = inb(0x61);

    if ((control & 3) != 3)
        outb(0x61, control | 3);
}

void speaker_off(void)
{
    outb(0x61, inb(0x61) & 0xFC);
}

static void wait_ms(unsigned int duration_ms)
{
    unsigned long start;
    unsigned long wait_ticks;

    wait_ticks = duration_ms / 10;

    if (wait_ticks == 0)
        wait_ticks = 1;

    start = get_ticks();

    while (get_ticks() - start < wait_ticks)
        asm volatile("hlt");
}

void speaker_beep(unsigned int frequency, unsigned int duration_ms)
{
    speaker_on(frequency);
    wait_ms(duration_ms);
    speaker_off();
}

void speaker_melody(void)
{
    static const unsigned int notes[] = {
        523, 523, 784, 784, 880, 880, 784,
        698, 698, 659, 659, 587, 587, 523
    };

    static const unsigned int lengths[] = {
        300, 300, 300, 300, 300, 300, 600,
        300, 300, 300, 300, 300, 300, 600
    };

    unsigned int i;

    for (i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {
        speaker_beep(notes[i], lengths[i]);
        wait_ms(40);
    }
}
