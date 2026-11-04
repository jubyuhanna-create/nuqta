; interrupt stubs. every vector pushes the same frame so C gets one
; struct regs no matter what fired. cpu pushes an error code for some
; exceptions and not others, so the others push a fake 0 to keep it even.

[bits 32]
extern isr_dispatch

%macro STUB_NOERR 1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro STUB_ERR 1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

section .text

%assign i 0
%rep 48
%if i = 8 || i = 10 || i = 11 || i = 12 || i = 13 || i = 14 || i = 17 || i = 21 || i = 29 || i = 30
    STUB_ERR i
%else
    STUB_NOERR i
%endif
%assign i i+1
%endrep

isr_common:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                    ; struct regs *
    call isr_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8                  ; int_no + err_code
    iret

section .data
global isr_table
isr_table:
%assign i 0
%rep 48
    dd isr %+ i
%assign i i+1
%endrep
