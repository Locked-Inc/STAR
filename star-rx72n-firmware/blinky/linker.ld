/*
 * blinky/linker.ld -- Minimal linker script for RX72N bare-metal blinky.
 *
 * RX72N memory map (R5F572NNHxFB, 144-pin):
 *   Internal RAM : 0x00000000 - 0x0007FFFF  (512 KB)
 *   Internal ROM : 0xFFE00000 - 0xFFFFFFFF  (2 MB active region)
 *
 * Note: RAM starts at 0x4 not 0x0. The first 4 bytes (0x0-0x3) are reserved
 * by the hardware (used as the undefined instruction trap vector on RX).
 *
 * Fixed hardware vector locations (RX CPU specification):
 *   0xFFFFFF80 - 0xFFFFFFFC : Exception/interrupt vector table
 *   0xFFFFFFFC - 0xFFFFFFFF : Reset vector (4 bytes, points to startup code)
 */

MEMORY
{
    RAM : ORIGIN = 0x00000004, LENGTH = 0x7FFFC    /* 512 KB - 4 bytes */
    ROM : ORIGIN = 0xFFE00000, LENGTH = 0x200000   /* 2 MB              */
}

SECTIONS
{
    /*
     * Fixed reset vector at the very top of flash.
     * The CPU reads this 32-bit address on power-on and jumps there.
     */
    .fvectors 0xFFFFFFFC : AT(0xFFFFFFFC)
    {
        KEEP(*(.fvectors))
    } > ROM

    /* Code and read-only data starting at base of ROM */
    .text 0xFFE00000 : AT(0xFFE00000)
    {
        *(.text*)
        *(.rodata*)
        etext = .;
    } > ROM

    /* Uninitialised data (BSS) -- main.c has no globals, so this is empty */
    .bss :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    /*
     * Stack definitions.
     * USP (user stack): top of RAM, grows downward.
     * ISP (interrupt stack): 2 KB below USP, grows downward.
     */
    _ustack = ORIGIN(RAM) + LENGTH(RAM);
    _istack = _ustack - 0x800;
}
