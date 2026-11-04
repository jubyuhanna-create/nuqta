ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text.entry)
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }

    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame) }
}
