_heap_stack_size = 8M;

SECTIONS
{
    . = 0xffff000000000000; /* kernel space */
    . += 0x80000;           /* kernel load address*/
    _start = .;
    .text : { *(.text.boot) *(.text) }
    .rodata : { *(rodata) }
    .data : { *(data)}
    .bss : {
        _bss_top = .;
        *(.bss)
    }
    _bss_size = SIZEOF(.bss) >> 3; /* byte to word */
    .heap : {
        . = ALIGN(8);
        _heap_top = .;
    }
    . = . + _heap_stack_size;
    .stack : {
        . = ALIGN(8);
        _stack_top = .;
    }
    _end = .;
}