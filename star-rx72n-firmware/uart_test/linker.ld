/*
 * gpio_test/linker.ld -- Linker script for GPIO breakout board test.
 *
 * Same memory map as usb_test: 512 KB internal SRAM, 2 MB internal ROM.
 * Includes Renesas-syntax sections (P, B, D, C) needed by ThreadX RXv3 port.
 */

MEMORY
{
    RAM : ORIGIN = 0x00000004, LENGTH = 0x7FFFC   /* 512 KB - 4 bytes */
    ROM : ORIGIN = 0xFFE00000, LENGTH = 0x200000  /* 2 MB             */
}

SECTIONS
{
    /* Code -- explicit ROM base so section VMA is anchored correctly. */
    .text 0xFFE00000 : AT(0xFFE00000)
    {
        *(.text*)
        *(P)
        *(P_1)
        *(.rodata*)
        *(C)
        *(C_1)
        *(C_2)
        etext = .;
    } > ROM

    /*
     * Relocatable vector table -- startup.S loads INTB with this address.
     * Must be 4-byte aligned; only vectors 27 (SWINT) and 28 (CMT0) are
     * used, but allocate 256 entries (1024 bytes) for safety.
     */
    .rvectors ALIGN(4) :
    {
        _rvectors_start = .;
        KEEP(*(.rvectors))
        _rvectors_end = .;
    } > ROM

    /*
     * Initialised data -- LMA stays in ROM, VMA in RAM.
     * startup.S copies _data..edata from _mdata on boot.
     */
    _mdata = .;
    .data : AT(_mdata)
    {
        _data = .;
        *(.data*)
        *(D)
        *(D_1)
        *(D_2)
        _edata = .;
    } > RAM

    /* BSS -- zeroed by startup.S */
    .bss :
    {
        _bss = .;
        *(.bss*)
        *(COMMON)
        *(B)
        *(B_1)
        *(B_2)
        _ebss = .;
        . = ALIGN(4);
        _end = .;
    } > RAM AT>RAM

    /*
     * Stacks -- USP grows down from top of RAM, ISP 4 KB below that.
     */
    _ustack = ORIGIN(RAM) + LENGTH(RAM);
    _istack = _ustack - 0x1000;

    /*
     * Exception vector table at fixed hardware address 0xFFFFFF80.
     * 32 entries -- all unused.
     */
    .exvectors 0xFFFFFF80 : AT(0xFFFFFF80)
    {
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF); LONG(0xFFFFFFFF); LONG(0xFFFFFFFF);
    } > ROM

    /* Fixed reset vector at 0xFFFFFFFC */
    .fvectors 0xFFFFFFFC : AT(0xFFFFFFFC)
    {
        KEEP(*(.fvectors))
    } > ROM
}
