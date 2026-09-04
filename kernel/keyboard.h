#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP 0x80
#define KEY_DOWN 0x81
#define KEY_LEFT 0x82
#define KEY_RIGHT 0x83

#define KEY_CTRL_S 0x13
#define KEY_CTRL_Q 0x11

extern char keyboard_buffer[256];
extern int keyboard_buffer_size;

int get_key(void);
void get_line(char *buffer, int size);
void get_init(void);
int key_available(void);
char keyboard_pop(void);
void init_keyboard(void);
void keyboard_handler(void);

#endif
