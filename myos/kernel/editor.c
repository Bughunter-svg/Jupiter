#include "screen.h"
#include "editor.h"
#include "keyboard.h"
#include "filesystem.h"
#include <stdint.h>

#define EDITOR_SIZE 512

static void draw_editor(const char *filename, const char *buffer, int cursor) {
    clear_screen();

    print("JupiterOS Editor - ");
    print(filename);
    print("\n");
    print("--------------------------------------------------\n");

    for (int i = 0; i < cursor; i++) {
        if (buffer[i] == '\n')
            print("\n");
        else
            print_char(buffer[i]);
    }

    print_char('_');

    print("\n");
    print("--------------------------------------------------\n");
    print("Ctrl+S Save    Ctrl+Q Save & Exit\n");
}

void launch_editor(const char *filename) {
    char buffer[EDITOR_SIZE];

    const char *existing = fs_read(filename);

    int length = 0;
    int cursor = 0;

    if (existing &&
        existing[0] != 'E') {
        while (existing[length] &&
               length < EDITOR_SIZE - 1) {
            buffer[length] = existing[length];
            length++;
        }
    }

    buffer[length] = '\0';
    cursor = length;

    draw_editor(filename, buffer, cursor);

    while (1) {
        int key = get_key();

        if (!key)
            continue;

        if (key == KEY_CTRL_S) {
            buffer[length] = '\0';
            fs_write(filename, buffer);
            draw_editor(filename, buffer, cursor);
            continue;
        }

        if (key == KEY_CTRL_Q) {
            buffer[length] = '\0';
            fs_write(filename, buffer);
            clear_screen();
            return;
        }

        if (key == '\b') {
            if (cursor > 0) {
                cursor--;
                length--;

                for (int i = cursor; i < length; i++)
                    buffer[i] = buffer[i + 1];

                buffer[length] = '\0';

                draw_editor(filename, buffer, cursor);
            }

            continue;
        }

        if (key == '\n' || key == '\r') {
            if (length < EDITOR_SIZE - 1) {
                for (int i = length; i > cursor; i--)
                    buffer[i] = buffer[i - 1];

                buffer[cursor] = '\n';
                cursor++;
                length++;

                buffer[length] = '\0';

                draw_editor(filename, buffer, cursor);
            }

            continue;
        }

        if (key >= 32 && key <= 126) {
            if (length < EDITOR_SIZE - 1) {
                for (int i = length; i > cursor; i--)
                    buffer[i] = buffer[i - 1];

                buffer[cursor] = (char)key;
                cursor++;
                length++;

                buffer[length] = '\0';

                draw_editor(filename, buffer, cursor);
            }
        }
    }
}
