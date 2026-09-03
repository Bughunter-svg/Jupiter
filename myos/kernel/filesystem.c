#include "filesystem.h"
#include "screen.h"
#include "string.h"
#include "memory.h"
#include <stddef.h>

typedef struct {
    char name[MAX_FILENAME];
    char *data;
    int  size;
    int  used;
} file_t;

static file_t files[MAX_FILES];

/* ─── init ───────────────────────────────────────────────────────── */
void fs_init(void) {
    for (int i = 0; i < MAX_FILES; i++) files[i].used = 0;
    print("File system initialized (RAM-FS)\n");
}

/* ─── live stats ─────────────────────────────────────────────────── */
int fs_get_file_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_FILES; i++) if (files[i].used) n++;
    return n;
}

int fs_get_bytes_used(void) {
    int b = 0;
    for (int i = 0; i < MAX_FILES; i++) if (files[i].used) b += files[i].size;
    return b;
}

/* ─── create ─────────────────────────────────────────────────────── */
int fs_create(const char *filename, const char *content) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, filename) == 0) {
            print("Error: File already exists\n");
            return 0;
        }
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            char *data = (char *)kmalloc(MAX_FILE_SIZE);

            if (!data) {
                print("Error: Not enough memory\n");
                return 0;
            }

            int name_len = strlen(filename);
            if (name_len >= MAX_FILENAME)
                name_len = MAX_FILENAME - 1;

            memcpy(files[i].name, filename, name_len);
            files[i].name[name_len] = '\0';

            int content_len = strlen(content);
            if (content_len >= MAX_FILE_SIZE)
                content_len = MAX_FILE_SIZE - 1;

            memcpy(data, content, content_len);
            data[content_len] = '\0';

            files[i].data = data;
            files[i].size = content_len;
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
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, filename) == 0) {
            int ct_len = strlen(content);

            if (ct_len >= MAX_FILE_SIZE)
                ct_len = MAX_FILE_SIZE - 1;

            memcpy(files[i].data, content, ct_len);
            files[i].data[ct_len] = '\0';
            files[i].size = ct_len;

            print("Saved: ");
            print(filename);
            print("\n");

            return 1;
        }
    }

    print("Error: File not found\n");
    return 0;
}
/* ─── read ───────────────────────────────────────────────────────── */
const char *fs_read(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++)
        if (files[i].used && strcmp(files[i].name, filename) == 0)
            return files[i].data;
    return "Error: File not found";
}

/* ─── delete ─────────────────────────────────────────────────────── */
int fs_delete(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, filename) == 0) {
            kfree(files[i].data);
            files[i].data = (char *)0;
            files[i].size = 0;
            files[i].used = 0;
            print("Deleted: ");
            print(filename);
            print("\n");
            return 1;
        }
    }
    print("Error: File not found\n");
    return 0;
}

/* ─── ls ─────────────────────────────────────────────────────────── */
void fs_list(void) {
    int found = 0;
    print("NAME                   SIZE\n");
    print("---------------------------\n");
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            char buf[8];
            print(files[i].name);
            /* Pad to column 23 */
            int pad = 23 - (int)strlen(files[i].name);
            while (pad-- > 0) print(" ");
            itoa(files[i].size, buf, 10); print(buf); print(" B\n");
            found++;
        }
    }
    if (!found) print("(no files)\n");
}

/* ─── append ─────────────────────────────────────────────────────── */
void fs_append(const char *filename, const char *content) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, filename) == 0) {
            int available = MAX_FILE_SIZE - 1 - files[i].size;
            int ct_len    = strlen(content);
            if (ct_len > available) ct_len = available;
            if (ct_len <= 0) { print("Error: File full\n"); return; }
            memcpy(files[i].data + files[i].size, content, ct_len);
            files[i].size            += ct_len;
            files[i].data[files[i].size] = '\0';
            print("Appended to "); print(filename); print("\n");
            return;
        }
    }
    print("Error: File not found\n");
}

/* ─── info ───────────────────────────────────────────────────────── */
void fs_info(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, filename) == 0) {
            char buf[8];
            print("File : "); print(filename); print("\n");
            print("Size : "); itoa(files[i].size, buf, 10); print(buf); print(" bytes\n");
            print("Data : \""); print(files[i].data); print("\"\n");
            return;
        }
    }
    print("Error: File not found\n");
}

/* ─── copy ───────────────────────────────────────────────────────── */
void fs_copy(const char *source, const char *dest) {
    const char *content = fs_read(source);
    if (strcmp(content, "Error: File not found") == 0) {
        print("Error: Source not found\n");
        return;
    }
    if (fs_create(dest, content)) {
        print("Copied "); print(source); print(" -> "); print(dest); print("\n");
    }
}

/* ─── edit (stub) ────────────────────────────────────────────────── */
int edit_file(const char *filename) {
    (void)filename;
    print("Editor: use 'create' / 'append' commands\n");
    return 0;
}
