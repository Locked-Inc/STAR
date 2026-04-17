/*
 * blinky_rtos/linker.ld -- Linker script for ThreadX-based multi-LED breathe.
 *
 * RX72N memory map (R5F572NNHxFB):
 *   Internal RAM  : 0x00000004 - 0x0007FFFF  (512 KB, first 4 bytes reserved)
 *   Internal ROM  : 0xFFE00000 - 0xFFFFFFFF  (2 MB)
 *
 * Fixed hardware vector locations:
 *   0xFFFFFF80 - 0xFFFFFFFC  Exception vector table (EXTB)
 *   0xFFFFFFFC - 0xFFFFFFFF  Reset vector (points to startup entry)
 *
 * INTB is loaded by startup.S to point at _rvectors_start (in ROM).
 *
 * Renesas-syntax sections from ThreadX RXv3 port (tx_timer_interrupt.S etc.):
 *   P, P_1  -- code sections (.SECTION P,CODE)
 *   B, B_1  -- BSS sections  (.SECTION B,DATA)
 *   D, D_1  -- initialised data sections
 */

MEMORY
{
    RAM : ORIGIN = 0x00000004, LENGTH = 0x7FFFC   /* 512 KB - 4 bytes */
    ROM : ORIGIN = 0xFFE00000, LENGTH = 0x200000  /* 2 MB             */
}

SECTIONS
{
    /* Code -- explicit ROM base so section VMA is anchored correctly.
     * *(P) captures Renesas-assembler code sections from ThreadX port files. */
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
     * Must be 4-byte aligned; 32 entries = 128 bytes.
     * No AT() needed: follows .text in ROM, VMA and LMA are the same.
     */
    .rvectors ALIGN(4) :
    {
        _rvectors_start = .;
        KEEP(*(.rvectors))
        _rvectors_end = .;
    } > ROM

    /*
     * Initialised data -- LMA stays in ROM (image stored here), VMA in RAM.
     * startup.S copies from _mdata (ROM) to _data.._edata (RAM) on boot.
     * Required for ThreadX: __tx_thread_system_state starts as 0xF0F0F0F0.
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

    /* BSS -- ThreadX needs this zeroed; startup.S handles it.
     * B/B_1/B_2 are Renesas-assembler BSS sections from ThreadX port files. */
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
        _end = .;           /* ThreadX tx_initialize_low_level reads _end */
    } > RAM AT>RAM

    /*
     * Stacks -- USP grows down from top of RAM, ISP 2 KB below that.
     * ThreadX uses the interrupt stack pointer (ISP) for ISR execution.
     */
    _ustack = ORIGIN(RAM) + LENGTH(RAM);
    _istack = _ustack - 0x800;

    /*
     * Exception vector table (EXTB) at fixed hardware address 0xFFFFFF80.
     */
    .exvectors 0xFFFFFF80 : AT(0xFFFFFF80)
    {
        LONG(0xFFFFFFFF);   /* BUSERR */
        LONG(0xFFFFFFFF);   /* reserved */
        LONG(0xFFFFFFFF);   /* FIFERR */
        LONG(0xFFFFFFFF);   /* FRDYI */
        LONG(0xFFFFFFFF);   /* reserved */
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
        LONG(0xFFFFFFFF);
    } > ROM

    /* Fixed reset vector at 0xFFFFFFFC */
    .fvectors 0xFFFFFFFC : AT(0xFFFFFFFC)
    {
        KEEP(*(.fvectors))
    } > ROM
}
