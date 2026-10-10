#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_LEFT   1
#define MOUSE_RIGHT  2
#define MOUSE_MIDDLE 4

int mouse_init(void);
void mouse_handler(void);
int mouse_is_ready(void);
int mouse_get_x(void);
int mouse_get_y(void);
int mouse_get_buttons(void);
void mouse_set_cursor_visible(int visible);
void mouse_set_paint(int enabled);

#endif
