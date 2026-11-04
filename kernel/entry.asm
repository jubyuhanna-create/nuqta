; first kernel code that runs. bootloader jumps to 0x10000 = _start.
; bss isnt part of the flat binary so we zero it ourselves before C sees it.

[bits 32]
section .text.entry

global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    mov esp, stack_top
    xor ebp, ebp                ; so stack traces stop here
    call kmain

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
