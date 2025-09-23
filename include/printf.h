//
// Created by 28109 on 2025/9/19.
//

#ifndef PRINTF_H
#define PRINTF_H

#include "types.h"

int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
void panic(const char *s);

#endif