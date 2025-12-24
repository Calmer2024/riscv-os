#ifndef TYPES_H
#define TYPES_H

#ifndef __ASSEMBLER__

#include <stdint.h>
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;

// 标准类型定义
typedef uint64_t size_t;
typedef int64_t ssize_t;

#endif // __ASSEMBLER__

#endif
