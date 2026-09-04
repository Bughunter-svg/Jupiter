#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stddef.h>

#define MAX_FILES 20
#define MAX_FILENAME 32
#define MAX_FILE_SIZE 512

void fs_init(void);
int fs_create(const char *filename, const char *content);
const char *fs_read(const char *filename);
int fs_delete(const char *filename);
void fs_list(void);
void fs_append(const char *filename, const char *content);
void fs_info(const char *filename);
void fs_copy(const char *source, const char *dest);
int fs_write(const char *filename, const char *content);
int edit_file(const char *filename);
int fs_get_file_count(void);
int fs_get_bytes_used(void);

#endif
