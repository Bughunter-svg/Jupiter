#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP 0x80
#define KEY_DOWN 0x81
#define KEY_LEFT 0x82
#define KEY_RIGHT 0x83

// Add these extern declarations
extern char keyboard_buffer[256];
extern int keyboard_buffer_size;

int get_key();
void get_line(char* buffer, int size);
void get_init();
int key_available();          // Check if key is available
char keyboard_pop();          // Get key from buffer
void init_keyboard();         // Initialize keyboard
void keyboard_handler(); 

#endif
