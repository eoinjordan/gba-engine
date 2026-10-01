.section .text.start,"ax",@progbits
.globl _start
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, __stack_top
    la t0, __bss_start
    la t1, __bss_end
1:  bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:  la t0, __fast_start
    la t1, __fast_end
4:  bgeu t0, t1, 5f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 4b
5:  call main
3:  j 3b
