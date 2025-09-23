//
// Created by 28109 on 2025/9/19.
//

#ifndef STRING_H
#define STRING_H

#include "types.h"

size_t strlen(const char *s);// 计算以'\0'结尾的字符串的长度
char *strcpy(char *dst, const char *src);// 将源字符串（包括终止符）复制到目标缓冲区

#endif