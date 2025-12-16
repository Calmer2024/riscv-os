// 使用内联汇编从 time CSR 中读取64位的时间计数值
unsigned long long get_time(void) {
    unsigned long long cycles;
    // "rdtime %0" 是汇编指令，把 time 寄存器的值读到 %0 (第一个操作数)
    // "=r" (cycles) 是约束，告诉编译器把结果存入 C 变量 cycles
    asm volatile("rdtime %0" : "=r" (cycles));
    return cycles;
}