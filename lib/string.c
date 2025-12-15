#include "string.h"

size_t strlen(const char* s)
{
    const char* p = s;
    while (*p) p++;
    return p - s;
}

char* strcpy(char* dst, const char* src)
{
    char* ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

void* memset(void* s, int c, size_t n)
{
    // 将通用指针 void* 转换为我们可以逐字节操作的 unsigned char*
    unsigned char* p = (unsigned char*)s;

    // 将传入的 int 值转换为要写入的字节
    unsigned char val = (unsigned char)c;

    // 循环 n 次，填充每个字节
    while (n--)
    {
        *p++ = val;
    }

    // 按照C语言标准，返回原始的指针
    return s;
}

void*
memcpy(void* dst, const void* src, size_t n)
{
    char* cdst = (char*)dst;
    const char* csrc = (const char*)src;
    for (size_t i = 0; i < n; i++)
    {
        cdst[i] = csrc[i];
    }
    return dst;
}

void*
memmove(void* dst, const void* src, uint n)
{
    const char* s;
    char* d;

    if (n == 0)
        return dst;

    s = src;
    d = dst;
    if (s < d && s + n > d)
    {
        s += n;
        d += n;
        while (n-- > 0)
            *--d = *--s;
    }
    else
        while (n-- > 0)
            *d++ = *s++;

    return dst;
}

char*
safestrcpy(char* s, const char* t, int n)
{
    char* os;

    os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0);
    *s = 0;
    return os;
}
