/**
 * @file rx72n.ld
 * @brief Linker script for Renesas RX72N (R5F572NNHGFP#30)
 *
 * Memory layout:
 * - ROM (Flash):  0xFFE00000 - 0xFFFFFFFF (4MB)
 * - RAM (SRAM):   0x00000000 - 0x000FFFFF (1MB)
 * - Data Flash:   0x00100000 - 0x00107FFF (32KB)
 */

/* Entry point */
ENTRY(_PowerON_Reset)

/* Memory regions for RX72N */
MEMORY
{
    /* Flash ROM: 4MB starting at 0xFFE00000 */
    ROM (rx)   : ORIGIN = 0xFFE00000, LENGTH = 4M

    /* RAM: 1MB starting at 0x00000000 */
    RAM (rwx)  : ORIGIN = 0x00000000, LENGTH = 1M

    /* Data Flash: 32KB */
    DFLASH (r) : ORIGIN = 0x00100000, LENGTH = 32K
}

/* Stack size configuration */
__STACK_SIZE = 0x4000;  /* 16KB for main stack */
__HEAP_SIZE  = 0x8000;  /* 32KB for heap */

SECTIONS
{
    /* =============================================================================
     * CODE SECTION (Flash ROM)
     * =============================================================================
     */

    /* Reset vector and interrupt vectors */
    .fvectors 0xFFFFFF80 :
    {
        KEEP(*(.fvectors))
    } > ROM

    /* Program code */
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.constdata)
        *(.constdata.*)

        /* C++ support */
        *(.init)
        *(.fini)
        *(.eh_frame)

        /* Keep ThreadX code */
        KEEP(*(.tx_*))

        . = ALIGN(4);
        _etext = .;
    } > ROM

    /* ARM exception tables (if any) */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > ROM

    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > ROM

    /* C++ constructors/destructors */
    .preinit_array :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > ROM

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > ROM

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > ROM

    /* =============================================================================
     * DATA SECTION (RAM, initialized from ROM)
     * =============================================================================
     */

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        __data_start__ = .;

        *(.data)
        *(.data.*)

        . = ALIGN(4);
        _edata = .;
        __data_end__ = .;
    } > RAM AT> ROM

    /* LMA (Load Memory Address) for .data initialization */
    _sidata = LOADADDR(.data);

    /* =============================================================================
     * BSS SECTION (RAM, zero-initialized)
     * =============================================================================
     */

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = .;
    } > RAM

    /* =============================================================================
     * HEAP SECTION
     * =============================================================================
     */

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_start__ = .;
        . = . + __HEAP_SIZE;
        . = ALIGN(8);
        __heap_end__ = .;
    } > RAM

    /* =============================================================================
     * STACK SECTION (grows downward from end of RAM)
     * =============================================================================
     */

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __stack_start__ = .;
        . = . + __STACK_SIZE;
        . = ALIGN(8);
        __stack_end__ = .;
        _StackTop = .;
    } > RAM

    /* End of RAM */
    _end = .;
    __end__ = .;

    /* =============================================================================
     * DEBUG SECTIONS (not loaded to target)
     * =============================================================================
     */

    /* DWARF debug sections */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    .debug_ranges   0 : { *(.debug_ranges) }

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu.lto_*)
    }
}
