@ Shamelessly ripped from Jeff Frohweins CRT0.s It's public domain as fat as
@ I know, so do with it what you want.

    .GLOBAL     InterruptProcess
    .GLOBAL     _start
_start:
        .ALIGN
        .CODE 32
	@ Start Vector

	b       rom_header_end

    @ Nintendo Logo Character Data (8000004h)
	.byte 0x24, 0x0FF, 0x0AE, 0x51, 0x69, 0x9A
	.byte 0x0A2, 0x21, 0x3D, 0x84, 0x82, 0x0A, 0x84, 0x0E4, 0x9, 0x0AD, 0x11
	.byte 0x24, 0x8B, 0x98, 0x0C0, 0x81, 0x7F, 0x21, 0x0A3, 0x52, 0x0BE
	.byte 0x19, 0x93, 0x9, 0x0CE, 0x20, 0x10, 0x46, 0x4A,0x4A, 0x0F8
	.byte 0x27, 0x31, 0x0EC, 0x58, 0x0C7, 0x0E8, 0x33, 0x82, 0x0E3, 0x0CE
	.byte 0x0BF, 0x85, 0x0F4, 0x0DF, 0x94, 0x0CE, 0x4B, 0x9, 0x0C1, 0x94
	.byte 0x56, 0x8A, 0x0C0, 0x13, 0x72, 0x0A7, 0x0FC, 0x9F, 0x84, 0x4D
	.byte 0x73, 0x0A3, 0x0CA, 0x9A, 0x61, 0x58, 0x97, 0x0A3, 0x27, 0x0FC
	.byte 0x3, 0x98, 0x76, 0x23, 0x1D, 0x0C7, 0x61, 0x3, 0x4, 0x0AE, 0x56, 0x0BF
	.byte 0x38, 0x84, 0, 0x40, 0x0A7, 0x0E, 0x0FD, 0x0FF, 0x52, 0x0FE
	.byte 0x3, 0x6F, 0x95, 0x30, 0x0F1, 0x97, 0x0FB, 0x0C0, 0x85, 0x60, 0x0D6
	.byte 0x80, 0x25, 0x0A9, 0x63, 0x0BE, 0x3, 0x1, 0x4E, 0x38, 0x0E2, 0x0F9
	.byte 0x0A2, 0x34, 0x0FF, 0x0BB, 0x3E, 0x3, 0x44, 0x78, 0x0, 0x90, 0x0CB
	.byte 0x88, 0x11, 0x3A, 0x94, 0x65, 0x0C0, 0x7C, 0x63, 0x87, 0x0F0
	.byte 0x3C, 0x0AF, 0x0D6, 0x25, 0x0E4, 0x8B, 0x38, 0x0A, 0x0AC, 0x72
	.byte 0x21, 0x0D4, 0x0F8, 0x7

    @ Game Title (80000A0h)
        .byte   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte   0x00,0x00,0x00,0x00

    @ Game Code (80000ACh)
        .byte   0x00,0x00,0x00,0x00

    @ Maker Code (80000B0h)
        .byte   0x30,0x31

    @ Fixed Value (80000B2h)
        .byte   0x96

    @ Main Unit Code (80000B3h)
        .byte   0x00

    @ Device Type (80000B4h)
        .byte   0x00

    @ Unused Data (7Byte) (80000B5h)
        .byte   0x00,0x00,0x00,0x00,0x00,0x00,0x00

    @ Software Version No (80000BCh)
        .byte   0x00

    @ Complement Check (80000BDh)
        .byte   0xf0

    @ Checksum (80000BEh)
        .byte   0x00,0x00

    .ALIGN
    .ARM                                @ ..or you can use CODE 32 here

rom_header_end:
        b       start_vector        @ This branch must be here for proper
                                    @ positioning of the following header.
                                    @ DO NOT REMOVE IT.
    .GLOBAL     __boot_method, __slave_number

__boot_method:
        .byte   0       @ boot method (0=ROM boot, 3=Multiplay boot)
__slave_number:
        .byte   0       @ slave # (1=slave#1, 2=slave#2, 3=slave#3)

        .byte   0       @ reserved
        .byte   0       @ reserved
        .word   0       @ reserved
        .word   0       @ reserved
        .word   0       @ reserved
        .word   0       @ reserved
        .word   0       @ reserved
        .word   0       @ reserved

    .GLOBAL     start_vector
    .ALIGN
    .ARM                                @ ..or you can use CODE 32 here
start_vector:
        mov     r0, #0x12               @ Switch to IRQ Mode
        msr     cpsr, r0
        ldr     sp,=__sp_irq            @ Set SP_irq
        mov     r0, #0x1f               @ Switch to System Mode
        msr     cpsr, r0
        ldr     sp,=__sp_usr            @ Set SP_usr



@ Enter Thumb mode
        adr    r0,1f + 1                @ add r0,pc,#1 also works here
                                        @  for those that want to conserve labels.
        bx     r0

        .THUMB                          @ ..or you can use .CODE 16 here
1:


@ Clear BSS section to 0x00
@ (Sometimes BSS may be in External WRAM)
        ldr     r0,=__bss_start
        ldr     r1,=__bss_end
        sub     r1,r0
        bl      ClearMem

@ Copy initialized data (data section) from LMA to VMA (ROM to RAM)
        ldr     r1,=__data_lma
        ldr     r2,=__data_start
        ldr     r4,=__data_end
        bl      CopyMemChk

@ Copy internal work ram (iwram section) from LMA to VMA (ROM to RAM)
        ldr     r1,=__iwram_lma
        ldr     r2,=__iwram_start
        ldr     r4,=__iwram_end
        bl      CopyMemChk


@ Enable ints
        ldr     r1, =__intr_vector_buf
        ldr     r0, =intr_main
        str     r0, [r1]

@ Jump to user code

        mov     r0,#0            @ int argc
        mov     r1,#0            @ char *argv[]

        ldr     r3,=start_vector
        mov     lr,r3            @ Set start_vector as return address

        ldr     r3,=main
        bx      r3

@ Clear memory to 0x00 if length != 0
@ r0 = Start Address
@ r1 = Length

ClearMem:
@        cmp     r1,#0           @ Is length zero?
@        beq     ClearMX         @ yes, exit

        mov     r2,#3            @ These commands are used in cases where
        add     r1,r2            @ the length is not a multiple of 4,
        bic     r1,r2            @ even though it should be.

        beq     ClearMX         @ Length is zero so exit

        mov     r2,#0
ClrLoop:
        stmia   r0!,{r2}
        sub     r1,#4
        bne     ClrLoop
ClearMX:
        bx      lr

@ Copy memory if length != 0
@ r1 = Source Address
@ r2 = Dest Address
@ r4 = Dest Address + Length

CopyMemChk:
        sub     r3,r4,r2         @ Is there any data to copy?
@        beq     CIDExit          @ no

@ Copy memory
@ r1 = Source Address
@ r2 = Dest Address
@ r3 = Length

CopyMem:
        mov     r0,#3            @ These commands are used in cases where
        add     r3,r0            @ the length is not a multiple of 4,
        bic     r3,r0            @ even though it should be.

        beq     CIDExit          @ Length is zero so exit

CIDLoop:
        ldmia   r1!,{r0}
        stmia   r2!,{r0}
        sub     r3,#4
        bne     CIDLoop
CIDExit:
If_Undefined_Reference__rename_main_or_AgbMain_to_each_other_in_your_C_file:
        bx      lr


    .GLOBAL     intr_main
    .ALIGN
    .ARM
intr_main:
        ldr     r0,=InterruptProcess
        bx      r0


        .ALIGN
        .POOL


    .EXTERN     IntrTable
    .EXTERN     InterruptProcess
    .GLOBAL     intr_main
    .ALIGN
    .ARM

    .ALIGN
    .POOL


    .END

