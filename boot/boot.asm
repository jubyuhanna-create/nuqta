; stage 1 boot sector. BIOS drops us here at 0x7c00 in real mode.
;
; what it does, in order:
;   1. read the kernel off the disk (LBA 1..KERNEL_SECTORS) to 0x10000
;   2. ask the BIOS for the memory map (e820) and leave it at 0x8000
;   3. open the A20 gate
;   4. load a flat GDT, flip CR0.PE, far jump into 32 bit code
;   5. jump to the kernel
;
; it has to fit in 510 bytes, so no fancy error handling. one letter and halt.

[org 0x7c00]
[bits 16]

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 64
%endif

KERNEL_SEG  equ 0x1000          ; 0x1000:0000 = 0x10000
MMAP_ADDR   equ 0x8000          ; u32 count, then 24 byte entries
MMAP_MAX    equ 32

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    mov [boot_drive], dl        ; BIOS tells us which drive we came from

    ; --- read kernel with int 13h extensions (LBA, no CHS math) ---
    mov ah, 0x41
    mov bx, 0x55aa
    mov dl, [boot_drive]
    int 0x13
    jc no_lba

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; --- e820 memory map ---
    mov di, MMAP_ADDR + 4
    xor ebx, ebx
    xor bp, bp
.e820_next:
    mov eax, 0xe820
    mov ecx, 24
    mov edx, 0x534d4150         ; 'SMAP'
    mov dword [es:di + 20], 1   ; ACPI 3 'valid' bit, some BIOSes want it
    int 0x15
    jc .e820_done
    cmp eax, 0x534d4150
    jne .e820_done
    inc bp
    add di, 24
    test ebx, ebx               ; ebx = 0 means that was the last entry
    jz .e820_done
    cmp bp, MMAP_MAX
    jb .e820_next
.e820_done:
    mov [MMAP_ADDR], bp
    mov word [MMAP_ADDR + 2], 0

    ; --- A20, the fast way through port 0x92 ---
    in al, 0x92
    or al, 2
    and al, 0xfe                ; bit 0 would reset the machine
    out 0x92, al

    ; --- protected mode ---
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry           ; far jump reloads CS and flushes the prefetch

no_lba:
    mov al, 'L'
    jmp die
disk_error:
    mov al, 'D'
die:
    mov ah, 0x0e
    int 0x10
.halt:
    hlt
    jmp .halt

[bits 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000            ; temporary, kernel sets its own
    jmp 0x08:0x10000

; --- data ---

boot_drive: db 0

align 4
dap:
    db 16, 0
    dw KERNEL_SECTORS
    dw 0x0000, KERNEL_SEG       ; offset, segment
    dq 1                        ; start at LBA 1, right after us

align 8
gdt:
    dq 0                        ; null descriptor
    ; code: base 0, limit 4G, ring 0, exec/read, 4K granularity, 32 bit
    dw 0xffff, 0x0000
    db 0x00, 0x9a, 0xcf, 0x00
    ; data: same thing but read/write
    dw 0xffff, 0x0000
    db 0x00, 0x92, 0xcf, 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

times 510 - ($ - $$) db 0
dw 0xaa55
