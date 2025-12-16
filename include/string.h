#ifndef STRING_H
#define STRING_H

#include "types.h"

// string.c
int sprintf(char *out, const char *fmt, ...);
void* memset(void* dst, int c, uint n);
char* strcpy(char *dst, const char *src);
int  strcmp(const char *s1, const char *s2);
void test_sprint_number(void);
void test_sprintf(void);
void* memmove(void *dst, const void *src, uint n);
int strlen(const char *s);
char* strncpy(char *s, const char *t, int n);


#endif //STRING_H
