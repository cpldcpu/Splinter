/* Linker script for NY8A050E1 (512 x 14-bit OTP, 32 B SRAM at 0x10..0x2F)    */
/* for Nyquest's LLVM toolchain (clang --target=ny8 -mcpu=ny8a051j).          */
/* ROM addresses are in bytes (2 bytes per instruction word).                 */

ENTRY(__start)

MEMORY {
  ROM   (rx) : ORIGIN = 0,        LENGTH = 0x3FC     /* 0x000..0x1FD words; 0x1FE/0x1FF reserved */
  BANK0 (rw) : ORIGIN = 0x400010, LENGTH = 0x20      /* GPR 0x10..0x2F */
}

SECTIONS {
  .reset     0x00 : { *(.reset)            } >ROM   /* word 0x000 */
  .interrupt 0x10 : { KEEP(*(.interrupt))  } >ROM   /* word 0x008 */
  /* words 0x009..0x00D are free; 0x00E/0x00F hold the factory rolling code */
  .text      0x20 : { *(.jtable) *(.lib .lib.*) *(.text .text.*) } >ROM   /* word 0x010 */
  .rodata         : { *(.rodata .rodata.*) } >ROM
  .checksum       : { *(.progck)           } >ROM
  .ram-bk0        : { *(.bss .bss.*)       } >BANK0
}
