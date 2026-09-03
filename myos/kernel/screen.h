#ifndef SCREEN_H
#define SCREEN_H

// Add these constants:
#define VIDEO_MEMORY_ADDRESS 0xB8000
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define WHITE_ON_BLACK 0x0F

void clear_screen();
void print(const char *str);
void print_at(const char *str, int col, int row);
void screen_set_cursor(int col, int row);
void screen_put_char(char c, int col, int row);
void screen_put_char_attr(char c, int col, int row, unsigned char attr);
void print_char(char c);
void print_hex(unsigned int num);
void print_int(int num);
void itoa(int num, char *buffer, int base);
void reverse(char *str);
void print_float(float num);

#endif
