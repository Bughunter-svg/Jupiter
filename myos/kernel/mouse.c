#include "mouse.h"
#include "ports.h"

#define PS2_DATA     0x60
#define PS2_STATUS   0x64
#define PS2_COMMAND  0x64

#define VIDEO_MEMORY ((volatile unsigned short *)0xB8000)
#define SCREEN_COLS  80
#define SCREEN_ROWS  25

#define COUNTS_PER_COLUMN 8
#define COUNTS_PER_ROW    16

static volatile int mouse_ready = 0;
static volatile int mouse_x = 40;
static volatile int mouse_y = 12;
static volatile int mouse_buttons = 0;

static int sub_x = 0;
static int sub_y = 0;
static int cycle = 0;
static unsigned char packet[3];

static int cursor_visible = 1;
static int paint_enabled = 0;
static int cursor_drawn = 0;
static int drawn_x = 0;
static int drawn_y = 0;
static unsigned char saved_attr = 0;
static unsigned char drawn_attr = 0;

static unsigned int irq_save(void)
{
    unsigned int flags;

    asm volatile(
        "pushf\n"
        "pop %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}

static void irq_restore(unsigned int flags)
{
    asm volatile(
        "push %0\n"
        "popf"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

static int wait_write(void)
{
    int timeout = 100000;

    while (timeout--) {
        if (!(inb(PS2_STATUS) & 0x02))
            return 1;
    }

    return 0;
}

static int wait_read(void)
{
    int timeout = 100000;

    while (timeout--) {
        if (inb(PS2_STATUS) & 0x01)
            return 1;
    }

    return 0;
}

static int mouse_write(unsigned char value)
{
    if (!wait_write())
        return 0;

    outb(PS2_COMMAND, 0xD4);

    if (!wait_write())
        return 0;

    outb(PS2_DATA, value);

    if (!wait_read())
        return 0;

    return inb(PS2_DATA) == 0xFA;
}

static void draw_cursor(void)
{
    volatile unsigned short *cell;
    unsigned short value;
    unsigned char attr;
    unsigned char inverted;

    cell = VIDEO_MEMORY + mouse_y * SCREEN_COLS + mouse_x;
    value = *cell;
    attr = (unsigned char)(value >> 8);

    inverted = (unsigned char)(((attr & 0x07) << 4) | ((attr >> 4) & 0x07));

    if (inverted == attr)
        inverted = 0x70;

    *cell = (unsigned short)((value & 0x00FF) | ((unsigned short)inverted << 8));

    saved_attr = attr;
    drawn_attr = inverted;
    drawn_x = mouse_x;
    drawn_y = mouse_y;
    cursor_drawn = 1;
}

static void erase_cursor(void)
{
    volatile unsigned short *cell;
    unsigned short value;

    if (!cursor_drawn)
        return;

    cell = VIDEO_MEMORY + drawn_y * SCREEN_COLS + drawn_x;
    value = *cell;

    if ((unsigned char)(value >> 8) == drawn_attr)
        *cell = (unsigned short)((value & 0x00FF) | ((unsigned short)saved_attr << 8));

    cursor_drawn = 0;
}

static void paint_cell(void)
{
    volatile unsigned short *cell;

    cell = VIDEO_MEMORY + mouse_y * SCREEN_COLS + mouse_x;

    if (mouse_buttons & MOUSE_LEFT)
        *cell = 0x4020;
    else if (mouse_buttons & MOUSE_RIGHT)
        *cell = 0x0720;
}

int mouse_init(void)
{
    unsigned char status;

    while (inb(PS2_STATUS) & 0x01)
        inb(PS2_DATA);

    if (!wait_write())
        return 0;

    outb(PS2_COMMAND, 0xA8);

    if (!wait_write())
        return 0;

    outb(PS2_COMMAND, 0x20);

    if (!wait_read())
        return 0;

    status = inb(PS2_DATA);
    status |= 0x02;
    status &= (unsigned char)~0x20;

    if (!wait_write())
        return 0;

    outb(PS2_COMMAND, 0x60);

    if (!wait_write())
        return 0;

    outb(PS2_DATA, status);

    if (!mouse_write(0xF6))
        return 0;

    if (!mouse_write(0xF4))
        return 0;

    cycle = 0;
    sub_x = 0;
    sub_y = 0;
    mouse_ready = 1;

    draw_cursor();

    return 1;
}

void mouse_handler(void)
{
    unsigned char status;
    unsigned char data;
    unsigned char flags;
    int dx;
    int dy;
    int cells;

    status = inb(PS2_STATUS);

    if (!(status & 0x01) || !(status & 0x20))
        return;

    data = inb(PS2_DATA);

    if (!mouse_ready)
        return;

    if (cycle == 0 && !(data & 0x08))
        return;

    packet[cycle++] = data;

    if (cycle < 3)
        return;

    cycle = 0;

    flags = packet[0];

    if (flags & 0xC0)
        return;

    dx = (int)packet[1] - (((int)flags << 4) & 0x100);
    dy = (int)packet[2] - (((int)flags << 3) & 0x100);

    mouse_buttons = flags & 0x07;

    erase_cursor();

    sub_x += dx;
    sub_y -= dy;

    cells = sub_x / COUNTS_PER_COLUMN;
    sub_x -= cells * COUNTS_PER_COLUMN;
    mouse_x += cells;

    cells = sub_y / COUNTS_PER_ROW;
    sub_y -= cells * COUNTS_PER_ROW;
    mouse_y += cells;

    if (mouse_x < 0)
        mouse_x = 0;

    if (mouse_x >= SCREEN_COLS)
        mouse_x = SCREEN_COLS - 1;

    if (mouse_y < 0)
        mouse_y = 0;

    if (mouse_y >= SCREEN_ROWS)
        mouse_y = SCREEN_ROWS - 1;

    if (paint_enabled && (mouse_buttons & (MOUSE_LEFT | MOUSE_RIGHT)))
        paint_cell();

    if (cursor_visible)
        draw_cursor();
}

int mouse_is_ready(void)
{
    return mouse_ready;
}

int mouse_get_x(void)
{
    return mouse_x;
}

int mouse_get_y(void)
{
    return mouse_y;
}

int mouse_get_buttons(void)
{
    return mouse_buttons;
}

void mouse_set_cursor_visible(int visible)
{
    unsigned int flags;

    flags = irq_save();

    cursor_visible = visible;

    if (!mouse_ready) {
        irq_restore(flags);
        return;
    }

    if (visible)
        draw_cursor();
    else
        erase_cursor();

    irq_restore(flags);
}

void mouse_set_paint(int enabled)
{
    paint_enabled = enabled;
}
