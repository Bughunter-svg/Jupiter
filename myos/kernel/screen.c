#include "screen.h"
#include "ports.h"

volatile unsigned char *video_memory = (unsigned char*)VIDEO_MEMORY_ADDRESS;
int cursor_x = 0;
int cursor_y = 0;

static void move_cursor(int col, int row) {
    unsigned short pos = row * SCREEN_WIDTH + col;
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    cursor_x = col;
    cursor_y = row;
}

static void scroll() {
    if (cursor_y >= SCREEN_HEIGHT) {
        int i;
        for (i = 0; i < (SCREEN_HEIGHT - 1) * SCREEN_WIDTH * 2; i++)
            video_memory[i] = video_memory[i + SCREEN_WIDTH * 2];
        for (i = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH * 2; i < SCREEN_HEIGHT * SCREEN_WIDTH * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i+1] = WHITE_ON_BLACK;
        }
        cursor_y = SCREEN_HEIGHT - 1;
        cursor_x = 0;
    }
}

static void put_char_at(char c, int col, int row, unsigned char attr) {
    unsigned int offset = (row * SCREEN_WIDTH + col) * 2;
    video_memory[offset] = c;
    video_memory[offset + 1] = attr;
}
void screen_set_cursor(int col, int row) {
    if (col < 0)
        col = 0;

    if (col >= SCREEN_WIDTH)
        col = SCREEN_WIDTH - 1;

    if (row < 0)
        row = 0;

    if (row >= SCREEN_HEIGHT)
        row = SCREEN_HEIGHT - 1;

    move_cursor(col, row);
}

void screen_put_char(char c, int col, int row) {
    if (col < 0 || col >= SCREEN_WIDTH)
        return;

    if (row < 0 || row >= SCREEN_HEIGHT)
        return;

    put_char_at(c, col, row, WHITE_ON_BLACK);
}

// --- public ---
void print_char(char c) {
    if (c == '\n') { cursor_x = 0; cursor_y++; }
    else if (c == '\r') { cursor_x = 0; }
    else if (c == '\b') {
        if (cursor_x > 0) { cursor_x--; put_char_at(' ', cursor_x, cursor_y, WHITE_ON_BLACK); }
        else if (cursor_y > 0) { cursor_y--; cursor_x = SCREEN_WIDTH - 1; put_char_at(' ', cursor_x, cursor_y, WHITE_ON_BLACK); }
    } else { put_char_at(c, cursor_x, cursor_y, WHITE_ON_BLACK); cursor_x++; }

    if (cursor_x >= SCREEN_WIDTH) { cursor_x = 0; cursor_y++; }
    scroll();
    move_cursor(cursor_x, cursor_y);
}
void screen_put_char_attr(char c, int col, int row, unsigned char attr) {
    if (col < 0 || col >= SCREEN_WIDTH)
        return;

    if (row < 0 || row >= SCREEN_HEIGHT)
        return;

    put_char_at(c, col, row, attr);
}

void clear_screen() {
    int i;
    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video_memory[i*2] = ' ';
        video_memory[i*2+1] = WHITE_ON_BLACK;
    }
    move_cursor(0,0);
}

void print(const char* message) {
    int i = 0;
    while (message[i]) print_char(message[i++]);
}

void print_at(const char* message, int col, int row) {
    move_cursor(col,row);
    print(message);
}

void print_float(float num) {
    // Handle negative numbers
    if (num < 0) {
        print("-");
        num = -num;
    }
    
    // Integer part
    int int_part = (int)num;
    print_int(int_part);
    
    // Decimal point
    print(".");
    
    // Fractional part (2 decimal places)
    float fractional = num - int_part;
    int decimal1 = (int)(fractional * 10) % 10;
    int decimal2 = (int)(fractional * 100) % 10;
    
    print_int(decimal1);
    print_int(decimal2);
}
