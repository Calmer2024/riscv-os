#ifndef RISCV_OS_ELF_H
#define RISCV_OS_ELF_H
#include "types.h"


// ELF 文件头 (File Header)
struct elfhdr {
    uint32 magic; // 必须是 ELF_MAGIC
    uint8  elf[12];
    uint16 type;
    uint16 machine;
    uint32 version;
    uint64 entry;   // 程序的入口虚拟地址
    uint64 phoff;   // 程序头表 (Program Header) 的文件偏移
    uint64 shoff;
    uint32 flags;
    uint16 ehsize;
    uint16 phentsize;
    uint16 phnum;   // 程序头表中有多少个条目
    uint16 shentsize;
    uint16 shnum;
    uint16 shstrndx;
};

// 程序头 (Program Header)
// 描述了 ELF 文件中一个需要被加载到内存的“段”(Segment)
struct proghdr {
    uint32 type;    // 段的类型 (e.g., PT_LOAD)
    uint32 flags;   // 段的权限 (Read/Write/Execute)
    uint64 off;     // 段在 ELF 文件中的偏移
    uint64 vaddr;   // 段应该被加载到的虚拟地址
    uint64 paddr;
    uint64 filesz;  // 段在文件中的大小
    uint64 memsz;   // 段在内存中的大小 (memsz >= filesz)
    uint64 align;
};

// ELF Magic Number
#define ELF_MAGIC 0x464C457FU // "\x7FELF" in little-endian

// Segment Type
#define ELF_PROG_LOAD 1

// Segment Flags
#define ELF_PROG_FLAG_EXEC  1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ  4

#endif //RISCV_OS_ELF_H