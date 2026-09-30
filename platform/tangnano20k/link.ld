ENTRY(_start)
MEMORY {
    RAM (rwx) : ORIGIN = 0, LENGTH = 896K
    FAST (rw) : ORIGIN = 0xe0000, LENGTH = 2K
}
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) } > RAM
    .rodata : { *(.rodata .rodata.* .srodata .srodata.*) } > RAM
    .data : {
        . = ALIGN(4); __global_pointer$ = . + 0x800;
        *(.data .data.* .sdata .sdata.*)
    } > RAM
    .bss (NOLOAD) : {
        . = ALIGN(4); __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*) *(COMMON)
        . = ALIGN(4); __bss_end = .;
    } > RAM
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
    .fast (NOLOAD) : {
        __fast_start = .; *(.fast) . = ALIGN(4); __fast_end = .;
    } > FAST
    ASSERT(__bss_end < __stack_top - 16384, "Engine exceeds RAM budget")
}
