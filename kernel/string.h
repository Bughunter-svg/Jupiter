#ifndef STRING_H
#define STRING_H

#include <stddef.h>

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int n);
int strlen(const char *str);
void strcpy(char *dest, const char *src);
void print_hex(unsigned int n);
void print_int(int n);
void itoa(int num, char *buffer, int base);
void reverse(char *str);
int atoi_simple(const char* str);
float atof_simple(const char* str);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
char* strcat(char* dest, const char* src);

#endif
