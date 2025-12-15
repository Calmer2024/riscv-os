//
// Created by 28109 on 2025/9/19.
//

#ifndef STRING_H
#define STRING_H

#include "types.h"

size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
void *memset(void *s, int c, size_t n);
void* memcpy(void *dst, const void *src, size_t n);
void* memmove(void *dst, const void *src, uint n);
char* safestrcpy(char* s, const char* t, int n);

#endif