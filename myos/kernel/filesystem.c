#include "filesystem.h"
#include "screen.h"
#include "string.h"
#include "memory.h"
#include <stddef.h>

typedef struct {
    char name[MAX_FILENAME];
    char *data;
    int size;
    int capacity;
    int used;
} file_t;

static file_t files[MAX_FILES];

static int file_capacity(int size) {
    if (size < 1)
        size = 1;

    return (size + 7) & ~7;
}

void fs_init(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = 0;
        files[i].data = (char *)0;
        files[i].size = 0;
        files[i].capacity = 0;
    }

    print("File system initialized (RAM-FS)\n");
}

int fs_get_file_count(void) {
    int n = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used)
            n++;
    }

    return n;
}

int fs_get_bytes_used(void) {
    int b = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used)
            b += files[i].size;
    }

    return b;
}

int fs_create(const char *filename, const char *content) {
    if (!filename || !content)
        return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used &&
            strcmp(files[i].name, filename) == 0) {
            print("Error: File already exists\n");
            return 0;
        }
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            int content_len = strlen(content);

            if (content_len >= MAX_FILE_SIZE)
                content_len = MAX_FILE_SIZE - 1;

            int capacity = file_capacity(content_len);

            char *data = (char *)kmalloc(capacity);

            if (!data) {
                print("Error: Not enough memory\n");
                return 0;
            }

            int name_len = strlen(filename);

            if (name_len >= MAX_FILENAME)
                name_len = MAX_FILENAME - 1;

            memcpy(files[i].name, filename, name_len);
            files[i].name[name_len] = '\0';

            memcpy(data, content, content_len);
            data[content_len] = '\0';

            files[i].data = data;
            files[i].size = content_len;
            files[i].capacity = capacity;
            files[i].used = 1;

            print("File created: ");
            print(filename);
            print("\n");

            return 1;
        }
    }

    print("Error: Maximum file limit reached\n");
    return 0;
}

int fs_write(const char *filename, const char *content) {
    if (!filename || !content)
        return 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used &&
            strcmp(files[i].name, filename) == 0) {

            int new_size = strlen(content);

            if (new_size >= MAX_FILE_SIZE)
                new_size = MAX_FILE_SIZE - 1;

            int new_capacity = file_capacity(new_size);

            if (new_capacity != files[i].capacity) {
                char *new_data = (char *)kmalloc(new_capacity);

                if (!new_data) {
                    print("Error: Not enough memory\n");
                    return 0;
                }

                memcpy(new_data, content, new_size);
                new_data[new_size] = '\0';

                kfree(files[i].data);

                files[i].data = new_data;
                files[i].capacity = new_capacity;
            } else {
                memcpy(files[i].data, content, new_size);
                files[i].data[new_size] = '\0';
            }

            files[i].size = new_size;

            print("Saved: ");
            print(filename);
            print("\n");

            return 1;
        }
    }

    print("Error: File not found\n");
    return 0;
}

const char *fs_read(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used &&
            strcmp(files[i].name, filename) == 0) {
            return files[i].data;
        }
    }

    return "Error: File not found";
}

int fs_delete(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used &&
            strcmp(files[i].name, filename) == 0) {

            kfree(files[i].data);

            files[i].data = (char *)0;
            files[i].size = 0;
            files[i].capacity = 0;
            files[i].used = 0;
            files[i].name[0] = '\0';

            print("Deleted: ");
            print(filename);
            print("\n");

            return 1;
        }
    }

    print("Error: File not found\n");
    return 0;
}

void fs_list(void) {
    int found = 0;

    print("NAME                   SIZE\n");
    print("---------------------------\n");

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            char buf[8];

            print(files[i].name);

            int pad = 23 - (int)strlen(files[i].name);

            while (pad-- > 0)
                print(" ");

            itoa(files[i].size, buf, 10);
            print(buf);
            print(" B\n");

            found++;
        }
    }

    if (!found)
        print("(no files)\n");
}

void fs_append(const char *filename, const char *content) {
    if (!filename || !content)
        return;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used &&
            strcmp(files[i].name, filename) == 0) {

            int old_size = files[i].size;
            int append_size = strlen(content);
            int new_size = old_size + append_size;

            if (new_size >= MAX_FILE_SIZE)
                new_size = MAX_FILE_SIZE - 1;

            if (new_size <= old_size) {
                print("Error: File full\n");
                return;
            }

            int new_capacity = file_capacity(new_size);

            if (new_capacity != files[i].capacity) {
                char *new_data = (char *)kmalloc(new_capacity);

                if (!new_data) {
                    print("Error: Not enough memory\n");
                    return;
                }

                memcpy(new_data, files[i].data, old_size);
                memcpy(
                    new_data + old_size,
                    content,
                    new_size - old_size
                );

                new_data[new_size] = '\0';

                kfree(files[i].data);

                files[i].data = new_data;
                files[i].capacity = new_capacity;
            } else {
                memcpy(
                    files[i].data + old_size,
                    content,
                    new_size - old_size
                );

                files[i].data[new_size] = '\0';
            }

            files[i].size = new_size;

            print("Appended to ");
            print(filename);
            print("\n");

            return;
        }
    }

    print("Error: File not found\n");
}

void fs_info(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used &&
            strcmp(files[i].name, filename) == 0) {

            char buf[8];

            print("File : ");
            print(filename);
            print("\n");

            print("Size : ");
            itoa(files[i].size, buf, 10);
            print(buf);
            print(" bytes\n");

            print("Capacity : ");
            itoa(files[i].capacity, buf, 10);
            print(buf);
            print(" bytes\n");

            print("Data : \"");
            print(files[i].data);
            print("\"\n");

            return;
        }
    }

    print("Error: File not found\n");
}

void fs_copy(const char *source, const char *dest) {
    const char *content = fs_read(source);

    if (strcmp(content, "Error: File not found") == 0) {
        print("Error: Source not found\n");
        return;
    }

    if (fs_create(dest, content)) {
        print("Copied ");
        print(source);
        print(" -> ");
        print(dest);
        print("\n");
    }
}

int edit_file(const char *filename) {
    (void)filename;
    print("Editor: use 'create' / 'append' commands\n");
    return 0;
}
