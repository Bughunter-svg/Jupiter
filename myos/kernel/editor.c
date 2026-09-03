#include "screen.h"
#include "editor.h"
#include "keyboard.h"
#include "filesystem.h"

#define EDITOR_SIZE 512
#define EDITOR_TOP 2
#define EDITOR_BOTTOM 22
#define EDITOR_ROWS 20

static void clear_editor_area(void) {
    for (int row = EDITOR_TOP; row < EDITOR_BOTTOM; row++) {
        for (int col = 0; col < SCREEN_WIDTH; col++)
            screen_put_char(' ', col, row);
    }
}

static int get_line_col(const char *buffer, int position) {
    int col = 0;

    for (int i = position - 1; i >= 0; i--) {
        if (buffer[i] == '\n')
            break;

        col++;
    }

    return col;
}

static int get_line_row(const char *buffer, int position) {
    int row = 0;

    for (int i = 0; i < position; i++) {
        if (buffer[i] == '\n')
            row++;
    }

    return row;
}

static int get_line_start(const char *buffer, int position) {
    int start = position;

    while (start > 0 && buffer[start - 1] != '\n')
        start--;

    return start;
}

static int get_line_end(const char *buffer, int length, int position) {
    int end = position;

    while (end < length && buffer[end] != '\n')
        end++;

    return end;
}

static void draw_editor(const char *filename,
                        const char *buffer,
                        int length,
                        int cursor) {
    clear_editor_area();

    print_at("JupiterOS Editor - ", 0, 0);

    int name_col = 19;

    for (int i = 0;
         filename[i] && name_col < SCREEN_WIDTH;
         i++, name_col++) {
        screen_put_char(filename[i], name_col, 0);
    }

    for (int col = 0; col < SCREEN_WIDTH; col++)
        screen_put_char('-', col, 1);

    int row = EDITOR_TOP;
    int col = 0;

    for (int i = 0; i < length; i++) {
        if (row >= EDITOR_BOTTOM)
            break;

        if (buffer[i] == '\n') {
            row++;
            col = 0;
            continue;
        }

        if (i == cursor)
            screen_put_char_attr(buffer[i], col, row, 0xF0);
        else
            screen_put_char(buffer[i], col, row);

        col++;

        if (col >= SCREEN_WIDTH) {
            col = 0;
            row++;
        }
    }

    int cursor_col = get_line_col(buffer, cursor);
    int cursor_row = get_line_row(buffer, cursor);

    if (cursor_row < EDITOR_ROWS) {
        if (cursor_col >= SCREEN_WIDTH)
            cursor_col = SCREEN_WIDTH - 1;

        if (cursor < length &&
            buffer[cursor] != '\n') {
            screen_put_char_attr(
                buffer[cursor],
                cursor_col,
                EDITOR_TOP + cursor_row,
                0xF0
            );
        } else {
            screen_put_char_attr(
                ' ',
                cursor_col,
                EDITOR_TOP + cursor_row,
                0xF0
            );
        }
    }

    for (int col2 = 0; col2 < SCREEN_WIDTH; col2++)
        screen_put_char('-', col2, EDITOR_BOTTOM);

    print_at("^S Save   ^Q Save & Exit   Arrows Move",
             0,
             EDITOR_BOTTOM + 1);

    screen_set_cursor(
        cursor_col,
        EDITOR_TOP + cursor_row
    );
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

    draw_editor(filename, buffer, length, cursor);

    while (1) {
        int key = get_key();

        if (!key)
            continue;

        if (key == KEY_CTRL_S) {
            buffer[length] = '\0';
            fs_write(filename, buffer);
            draw_editor(filename, buffer, length, cursor);
            continue;
        }

        if (key == KEY_CTRL_Q) {
            buffer[length] = '\0';
            fs_write(filename, buffer);
            clear_screen();
            return;
        }

        if (key == KEY_LEFT) {
            if (cursor > 0)
                cursor--;

            draw_editor(filename, buffer, length, cursor);
            continue;
        }

        if (key == KEY_RIGHT) {
            if (cursor < length)
                cursor++;

            draw_editor(filename, buffer, length, cursor);
            continue;
        }

        if (key == KEY_UP) {
            int current_col = get_line_col(buffer, cursor);
            int current_start = get_line_start(buffer, cursor);

            if (current_start > 0) {
                int previous_position = current_start - 1;
                int previous_start =
                    get_line_start(buffer, previous_position);

                int previous_end =
                    get_line_end(buffer, length, previous_position);

                int previous_length =
                    previous_end - previous_start;

                if (current_col > previous_length)
                    current_col = previous_length;

                cursor = previous_start + current_col;
            }

            draw_editor(filename, buffer, length, cursor);
            continue;
        }

        if (key == KEY_DOWN) {
            int current_col = get_line_col(buffer, cursor);
            int current_end = get_line_end(buffer, length, cursor);

            if (current_end < length) {
                int next_start = current_end + 1;
                int next_end =
                    get_line_end(buffer, length, next_start);

                int next_length =
                    next_end - next_start;

                if (current_col > next_length)
                    current_col = next_length;

                cursor = next_start + current_col;
            }

            draw_editor(filename, buffer, length, cursor);
            continue;
        }

        if (key == '\b') {
            if (cursor > 0) {
                for (int i = cursor - 1; i < length - 1; i++)
                    buffer[i] = buffer[i + 1];

                cursor--;
                length--;

                buffer[length] = '\0';

                draw_editor(filename, buffer, length, cursor);
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

                draw_editor(filename, buffer, length, cursor);
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

                draw_editor(filename, buffer, length, cursor);
            }
        }
    }
}
