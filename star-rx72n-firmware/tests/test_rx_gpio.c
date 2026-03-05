/* SPDX-License-Identifier: MIT */
/**
 * @file test_rx_gpio.c
 * @brief Unit Tests for RX72N GPIO (PORT) Register Definitions
 *
 * @details
 * Comprehensive verification tests for the RX72N I/O Ports module (Chapter 22
 * of the hardware manual). This test suite validates:
 *
 * - **Register base addresses**: PDR, PODR, PIDR, PMR, ODR, PCR, DSCR bases
 * - **Port offset values**: Numeric ports (0-5), alpha ports (A-E), and Port J
 * - **Struct layout**: rx_port_regs_t field offsets match hardware spacing
 * - **Accessor functions**: All port-specific accessor functions return correct
 *   hardware addresses
 * - **ODR register layout**: Special 16-bit access for open-drain control
 * - **Field widths**: 8-bit registers except 16-bit ODR
 *
 * @par Test Coverage Summary
 * | Test Category          | Tests | Description                         |
 * |------------------------|-------|-------------------------------------|
 * | Base Addresses         | 1     | 7 register type bases verified      |
 * | Port Offsets           | 1     | 12 port offsets verified            |
 * | Struct Layout          | 2     | Field offsets and struct size       |
 * | Accessor Functions     | 9     | All port accessor functions         |
 * | ODR Layout             | 1     | 16-bit ODR register access          |
 * | Field Widths           | 1     | All register field sizes            |
 *
 * @par Test Framework
 * Uses Unity test framework (ThrowTheSwitch.org).
 * Tests are run on host machine (x86/x64), not on target hardware.
 *
 * @par Verification Methodology
 * Each test compares computed addresses against values from:
 * - RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 22: I/O Ports, Table 22.1
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: Simple control flow (no goto, setjmp, recursion)
 * - Rule 2: N/A (no loops with iterations)
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: Functions are short (< 60 lines each)
 * - Rule 5: Minimal assertions per test (1-7 per function)
 * - Rule 6: Minimal variable scope
 * - Rule 7: Return values verified via Unity assertions
 * - Rule 8: Limited preprocessor usage
 * - Rule 9: No function pointers in tests
 * - Rule 10: Compiled with -Wall -Wextra -Werror
 *
 * @par Module Dependencies
 * - rx72n_port_regs.h: Register definitions under test
 * - unity.h: Test framework
 * - stddef.h, stdint.h: Standard type definitions
 *
 * @par Build Command
 * @code{.sh}
 * cd star-rx72n-firmware/tests
 * cmake -B build && cmake --build build && ctest --test-dir build -V
 * @endcode
 *
 * @see rx72n_port_regs.h GPIO register definitions
 * @see Ch22_IO_Ports.txt Manual chapter extract
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * @since Version 1.0.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rx72n_port_regs.h"
#include "unity.h"

/* =============================================================================
 * Test Setup and Teardown
 * =============================================================================
 */

/**
 * @brief Unity test setup fixture - called before each test
 *
 * @details
 * Initializes test environment. Currently no state to reset since tests
 * only verify constant addresses and struct layouts.
 *
 * @note Called automatically by Unity framework via RUN_TEST() macro
 *
 * @since Version 1.0.0
 */
void setUp(void)
{
  /* No state to initialize - tests are address verification only */
}

/**
 * @brief Unity test teardown fixture - called after each test
 *
 * @details
 * Cleans up test environment. Currently no cleanup required.
 *
 * @note Called automatically by Unity framework after each test
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  /* No cleanup required */
}

/* =============================================================================
 * Register Base Address Tests
 * =============================================================================
 */

/**
 * @brief Verify PORT register base addresses match RX72N hardware manual Ch22
 *
 * @details
 * Validates all 7 register type base addresses against Table 22.1 of the
 * RX72N hardware manual. The I/O port registers are memory-mapped starting
 * at 0x0008C000 with 32-byte spacing between register types.
 *
 * @par Register Base Address Map (Ch22 Table 22.1)
 * | Register | Base Address | Description                         |
 * |----------|--------------|-------------------------------------|
 * | PDR      | 0x0008C000   | Port Direction Register (input/out) |
 * | PODR     | 0x0008C020   | Port Output Data Register           |
 * | PIDR     | 0x0008C040   | Port Input Data Register            |
 * | PMR      | 0x0008C060   | Port Mode Register (GPIO/periph)    |
 * | ODR0     | 0x0008C080   | Open Drain Control Register 0       |
 * | PCR      | 0x0008C0C0   | Pull-up Control Register            |
 * | DSCR     | 0x0008C0E0   | Drive Capacity Control Register     |
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies k_port_pdr_base through k_port_dscr_base enum values
 *
 * @since Version 1.0.0
 */
void test_port_base_addresses(void)
{
  /* PDR (Port Direction Register) base */
  TEST_ASSERT_EQUAL_HEX32(0x0008C000, k_port_pdr_base);

  /* PODR (Port Output Data Register) base */
  TEST_ASSERT_EQUAL_HEX32(0x0008C020, k_port_podr_base);

  /* PIDR (Port Input Data Register) base */
  TEST_ASSERT_EQUAL_HEX32(0x0008C040, k_port_pidr_base);

  /* PMR (Port Mode Register) base */
  TEST_ASSERT_EQUAL_HEX32(0x0008C060, k_port_pmr_base);

  /* ODR0 (Open Drain Control 0) base */
  TEST_ASSERT_EQUAL_HEX32(0x0008C080, k_port_odr0_base);

  /* PCR (Pull-up Control Register) base */
  TEST_ASSERT_EQUAL_HEX32(0x0008C0C0, k_port_pcr_base);

  /* DSCR (Drive Capacity Control Register) base */
  TEST_ASSERT_EQUAL_HEX32(0x0008C0E0, k_port_dscr_base);
}

/* =============================================================================
 * Port Offset Tests (144-pin LFQFP)
 * =============================================================================
 */

/**
 * @brief Verify port offset values for 144-pin LFQFP package
 *
 * @details
 * Each port has a fixed offset from the register base address. The RX72N
 * 144-pin LFQFP package supports ports 0-9, A-E, and J. Port offsets are
 * added to register base addresses to get the specific port's register.
 *
 * @par Port Offset Calculation Example
 * @code
 * PORT3.PDR = PDR_base + PORT3_offset = 0x0008C000 + 0x03 = 0x0008C003
 * PORTB.PODR = PODR_base + PORTB_offset = 0x0008C020 + 0x0B = 0x0008C02B
 * @endcode
 *
 * @par Available Ports (144-pin LFQFP)
 * | Port  | Offset | Pins Available              |
 * |-------|--------|------------------------------|
 * | 0     | 0x00   | P00-P07 (8 pins)            |
 * | 1     | 0x01   | P10-P17 (8 pins)            |
 * | 2     | 0x02   | P20-P27 (8 pins)            |
 * | 3     | 0x03   | P30-P37 (8 pins)            |
 * | 4     | 0x04   | P40-P47 (8 pins)            |
 * | 5     | 0x05   | P50-P57 (8 pins)            |
 * | 6     | 0x06   | P60-P67 (partial)           |
 * | 7     | 0x07   | P70-P77 (8 pins)            |
 * | 8     | 0x08   | P80-P87 (partial)           |
 * | 9     | 0x09   | P90-P97 (8 pins)            |
 * | A     | 0x0A   | PA0-PA7 (8 pins)            |
 * | B     | 0x0B   | PB0-PB7 (8 pins)            |
 * | C     | 0x0C   | PC0-PC7 (8 pins)            |
 * | D     | 0x0D   | PD0-PD7 (8 pins)            |
 * | E     | 0x0E   | PE0-PE7 (8 pins)            |
 * | F     | 0x0F   | PF0-PF5 (partial)           |
 * | J     | 0x12   | PJ3, PJ5 (partial)          |
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies k_port_offset_0 through k_port_offset_j enum values
 *
 * @since Version 1.0.0
 */
void test_port_offsets(void)
{
  /* Numeric ports (0-5) */
  TEST_ASSERT_EQUAL_UINT8(0x00, k_port_offset_0);
  TEST_ASSERT_EQUAL_UINT8(0x01, k_port_offset_1);
  TEST_ASSERT_EQUAL_UINT8(0x02, k_port_offset_2);
  TEST_ASSERT_EQUAL_UINT8(0x03, k_port_offset_3);
  TEST_ASSERT_EQUAL_UINT8(0x04, k_port_offset_4);
  TEST_ASSERT_EQUAL_UINT8(0x05, k_port_offset_5);

  /* Alpha ports (A-E) */
  TEST_ASSERT_EQUAL_UINT8(0x0A, k_port_offset_a);
  TEST_ASSERT_EQUAL_UINT8(0x0B, k_port_offset_b);
  TEST_ASSERT_EQUAL_UINT8(0x0C, k_port_offset_c);
  TEST_ASSERT_EQUAL_UINT8(0x0D, k_port_offset_d);
  TEST_ASSERT_EQUAL_UINT8(0x0E, k_port_offset_e);

  /* Port J */
  TEST_ASSERT_EQUAL_UINT8(0x12, k_port_offset_j);
}

/* =============================================================================
 * Struct Layout Tests
 * =============================================================================
 */

/**
 * @brief Verify rx_port_regs_t struct field offsets match hardware layout
 *
 * @details
 * The rx_port_regs_t structure mirrors the hardware register layout for a
 * single port. Field offsets must match the 32-byte spacing between register
 * types in hardware. This test uses offsetof() to verify correct padding.
 *
 * @par Expected Struct Layout
 * | Offset | Field | Size   | Description                      |
 * |--------|-------|--------|----------------------------------|
 * | 0x00   | pdr   | 1 byte | Port Direction Register          |
 * | 0x20   | podr  | 1 byte | Port Output Data Register        |
 * | 0x40   | pidr  | 1 byte | Port Input Data Register         |
 * | 0x60   | pmr   | 1 byte | Port Mode Register               |
 * | 0x80   | odr   | 2 bytes| Open Drain Control (ODR0+ODR1)   |
 * | 0xC0   | pcr   | 1 byte | Pull-up Control Register         |
 * | 0xE0   | dscr  | 1 byte | Drive Capacity Control Register  |
 *
 * @note Padding bytes between fields are required to achieve 32-byte alignment
 *
 * @pre None
 * @post No state changes
 *
 * @test Uses offsetof() macro to verify compile-time field positions
 *
 * @since Version 1.0.0
 */
void test_port_struct_offsets(void)
{
  /* Verify each register offset within the struct */
  /* Note: ODR is not in struct - uses word-addressed accessor functions */
  TEST_ASSERT_EQUAL_size_t(0x00, offsetof(rx_port_regs_t, pdr));
  TEST_ASSERT_EQUAL_size_t(0x20, offsetof(rx_port_regs_t, podr));
  TEST_ASSERT_EQUAL_size_t(0x40, offsetof(rx_port_regs_t, pidr));
  TEST_ASSERT_EQUAL_size_t(0x60, offsetof(rx_port_regs_t, pmr));
  TEST_ASSERT_EQUAL_size_t(0xC0, offsetof(rx_port_regs_t, pcr));
  TEST_ASSERT_EQUAL_size_t(0xE0, offsetof(rx_port_regs_t, dscr));
}

/**
 * @brief Verify rx_port_regs_t struct total size is correct
 *
 * @details
 * The structure spans from PDR (0x00) to DSCR (0xE0) plus 1 byte for DSCR
 * itself, giving a total size of 0xE1 (225) bytes. This size ensures the
 * structure correctly covers all register addresses for a single port.
 *
 * @par Size Calculation
 * @code
 * struct_size = last_field_offset + last_field_size
 *             = 0xE0 (DSCR offset) + 1 (DSCR size)
 *             = 0xE1 = 225 bytes
 * @endcode
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies sizeof(rx_port_regs_t) == 0x129 (297 bytes)
 *
 * @note Updated from 0xE1 (225 bytes) to include DSCR2 register at +0x128
 *
 * @since Version 1.0.0
 */
void test_port_struct_size(void)
{
  /* Struct size should be 0x129 (297 bytes) - includes DSCR2 register */
  TEST_ASSERT_EQUAL_size_t(0x129, sizeof(rx_port_regs_t));
}

/* =============================================================================
 * Accessor Function Tests
 * =============================================================================
 */

/**
 * @brief Verify PORT0 accessor function returns correct hardware addresses
 *
 * @details
 * Tests the port0() inline accessor function which provides type-safe access
 * to PORT0 registers. Verifies both the base pointer address and derived
 * register addresses through struct member access.
 *
 * @par PORT0 Register Addresses (144-pin LFQFP)
 * | Register    | Address    | Description              |
 * |-------------|------------|--------------------------|
 * | PORT0.PDR   | 0x0008C000 | Direction (0=in, 1=out)  |
 * | PORT0.PODR  | 0x0008C020 | Output data              |
 * | PORT0.PIDR  | 0x0008C040 | Input data (read-only)   |
 * | PORT0.PMR   | 0x0008C060 | Mode (0=GPIO, 1=periph)  |
 * | PORT0.ODR   | 0x0008C080 | Open drain control       |
 * | PORT0.PCR   | 0x0008C0C0 | Pull-up enable           |
 * | PORT0.DSCR  | 0x0008C0E0 | Drive strength           |
 *
 * @pre None
 * @post No state changes (read-only address verification)
 *
 * @test Verifies port0() == 0x0008C000 and all derived register addresses
 *
 * @since Version 1.0.0
 */
void test_port0_accessor(void)
{
  volatile rx_port_regs_t* p = port0();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C000, (void*)p);

  /* Verify individual register addresses */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C000, (void*)&p->pdr);     /* PDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C020, (void*)&p->podr);    /* PODR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C040, (void*)&p->pidr);    /* PIDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C060, (void*)&p->pmr);     /* PMR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C080, (void*)port0_odr()); /* ODR (word-addressed) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0C0, (void*)&p->pcr);     /* PCR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0E0, (void*)&p->dscr);    /* DSCR */
}

/**
 * @brief Verify PORT1 accessor function returns correct hardware addresses
 *
 * @details
 * Tests the port1() inline accessor function. PORT1 is at offset 0x01 from
 * the base, so PORT1.PDR is at 0x0008C001, etc. Note that ODR uses 16-bit
 * addressing, so PORT1.ODR is at 0x0008C082 (2-byte offset from PORT0.ODR).
 *
 * @par PORT1 Register Addresses
 * | Register    | Address    | Notes                    |
 * |-------------|------------|--------------------------|
 * | PORT1.PDR   | 0x0008C001 | Base + 0x01              |
 * | PORT1.PODR  | 0x0008C021 | Base + 0x20 + 0x01       |
 * | PORT1.PIDR  | 0x0008C041 | Base + 0x40 + 0x01       |
 * | PORT1.PMR   | 0x0008C061 | Base + 0x60 + 0x01       |
 * | PORT1.ODR   | 0x0008C082 | Base + 0x80 + 0x02 (16b) |
 * | PORT1.PCR   | 0x0008C0C1 | Base + 0xC0 + 0x01       |
 * | PORT1.DSCR  | 0x0008C0E1 | Base + 0xE0 + 0x01       |
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies port1() returns correct base and register addresses
 *
 * @since Version 1.0.0
 */
void test_port1_accessor(void)
{
  volatile rx_port_regs_t* p = port1();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C001, (void*)p);

  /* Verify individual register addresses */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C001, (void*)&p->pdr);     /* PDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C021, (void*)&p->podr);    /* PODR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C041, (void*)&p->pidr);    /* PIDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C061, (void*)&p->pmr);     /* PMR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C082, (void*)port1_odr()); /* ODR (word-addressed) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0C1, (void*)&p->pcr);     /* PCR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0E1, (void*)&p->dscr);    /* DSCR */
}

/**
 * @brief Verify PORT2 accessor function returns correct hardware addresses
 *
 * @details
 * Tests the port2() inline accessor function. PORT2 is commonly used for
 * peripheral functions in the STAR project.
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies port2() == 0x0008C002 and key register addresses
 *
 * @since Version 1.0.0
 */
void test_port2_accessor(void)
{
  volatile rx_port_regs_t* p = port2();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C002, (void*)p);

  /* Verify key registers */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C002, (void*)&p->pdr);  /* PDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C022, (void*)&p->podr); /* PODR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C042, (void*)&p->pidr); /* PIDR */
}

/**
 * @brief Verify PORTA accessor function returns correct hardware addresses
 *
 * @details
 * Tests the porta() inline accessor function. PORTA (offset 0x0A) is the first
 * alphabetic port, used for various peripheral connections in the STAR project.
 * Note that ODR addressing for alpha ports uses 2-byte steps: PORTA.ODR is at
 * 0x0008C094 (base + 0x80 + 0x0A*2 = 0x0008C094).
 *
 * @par PORTA Register Addresses
 * | Register    | Address    | Notes                    |
 * |-------------|------------|--------------------------|
 * | PORTA.PDR   | 0x0008C00A | GPIO direction           |
 * | PORTA.PODR  | 0x0008C02A | Output data              |
 * | PORTA.PIDR  | 0x0008C04A | Input data               |
 * | PORTA.PMR   | 0x0008C06A | Peripheral mode          |
 * | PORTA.ODR   | 0x0008C094 | Open drain (0x80+0x14)   |
 * | PORTA.PCR   | 0x0008C0CA | Pull-up control          |
 * | PORTA.DSCR  | 0x0008C0EA | Drive strength           |
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies porta() returns correct base and all register addresses
 *
 * @since Version 1.0.0
 */
void test_porta_accessor(void)
{
  volatile rx_port_regs_t* p = porta();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00A, (void*)p);

  /* Verify individual register addresses */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00A, (void*)&p->pdr);     /* PDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C02A, (void*)&p->podr);    /* PODR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C04A, (void*)&p->pidr);    /* PIDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C06A, (void*)&p->pmr);     /* PMR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C094, (void*)porta_odr()); /* ODR (word-addressed) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0CA, (void*)&p->pcr);     /* PCR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0EA, (void*)&p->dscr);    /* DSCR */
}

/**
 * @brief Verify PORTB accessor function returns correct hardware addresses
 *
 * @details
 * Tests portb() accessor. PORTB (offset 0x0B) is used for ELC event ports
 * and general I/O in the STAR project.
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies portb() == 0x0008C00B and key register addresses
 *
 * @since Version 1.0.0
 */
void test_portb_accessor(void)
{
  volatile rx_port_regs_t* p = portb();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00B, (void*)p);

  /* Verify key registers */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00B, (void*)&p->pdr);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C02B, (void*)&p->podr);
}

/**
 * @brief Verify PORTC accessor function returns correct hardware addresses
 *
 * @details
 * Tests portc() accessor. PORTC (offset 0x0C) is commonly used for
 * communication peripherals (SCI, SPI).
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies portc() == 0x0008C00C and key register addresses
 *
 * @since Version 1.0.0
 */
void test_portc_accessor(void)
{
  volatile rx_port_regs_t* p = portc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00C, (void*)p);

  /* Verify key registers */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00C, (void*)&p->pdr);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C02C, (void*)&p->podr);
}

/**
 * @brief Verify PORTD accessor function returns correct hardware addresses
 *
 * @details
 * Tests portd() accessor. PORTD (offset 0x0D) is used for ADC inputs and
 * general purpose I/O.
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies portd() == 0x0008C00D and key register addresses
 *
 * @since Version 1.0.0
 */
void test_portd_accessor(void)
{
  volatile rx_port_regs_t* p = portd();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00D, (void*)p);

  /* Verify key registers */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00D, (void*)&p->pdr);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C02D, (void*)&p->podr);
}

/**
 * @brief Verify PORTE accessor function returns correct hardware addresses
 *
 * @details
 * Tests porte() accessor. PORTE (offset 0x0E) is the last alphabetic port,
 * used for ELC event ports and general I/O.
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies porte() == 0x0008C00E and key register addresses
 *
 * @since Version 1.0.0
 */
void test_porte_accessor(void)
{
  volatile rx_port_regs_t* p = porte();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00E, (void*)p);

  /* Verify key registers */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C00E, (void*)&p->pdr);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C02E, (void*)&p->podr);
}

/**
 * @brief Verify PORTJ accessor function returns correct hardware addresses
 *
 * @details
 * Tests portj() accessor. PORTJ (offset 0x12) is a special-purpose port
 * with limited pins (PJ3 and PJ5 available on 144-pin LFQFP). PJ3 is the
 * E2 emulator debug pin (MD/FINED function).
 *
 * @par PORTJ Register Addresses
 * | Register    | Address    | Notes                    |
 * |-------------|------------|--------------------------|
 * | PORTJ.PDR   | 0x0008C012 | Direction for PJ3 only   |
 * | PORTJ.PODR  | 0x0008C032 | Output data              |
 * | PORTJ.PIDR  | 0x0008C052 | Input data               |
 * | PORTJ.PMR   | 0x0008C072 | Mode register            |
 * | PORTJ.ODR   | 0x0008C0A4 | Open drain (0x80+0x24)   |
 * | PORTJ.PCR   | 0x0008C0D2 | Pull-up control          |
 * | PORTJ.DSCR  | 0x0008C0F2 | Drive strength           |
 *
 * @warning Only PJ3 and PJ5 are available on 144-pin package - other bits reserved
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies portj() returns correct base and all register addresses
 *
 * @since Version 1.0.0
 */
void test_portj_accessor(void)
{
  volatile rx_port_regs_t* p = portj();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C012, (void*)p);

  /* Verify individual register addresses */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C012, (void*)&p->pdr);     /* PDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C032, (void*)&p->podr);    /* PODR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C052, (void*)&p->pidr);    /* PIDR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C072, (void*)&p->pmr);     /* PMR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0A4, (void*)portj_odr()); /* ODR (word-addressed) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0D2, (void*)&p->pcr);     /* PCR */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C0F2, (void*)&p->dscr);    /* DSCR */
}

/* =============================================================================
 * ODR Register Tests
 * =============================================================================
 */

/**
 * @brief Verify ODR register 16-bit layout and inter-port spacing
 *
 * @details
 * The Open Drain Control Register (ODR) is unique among port registers in
 * that it uses 16-bit access. Each port has two 8-bit registers (ODR0 for
 * bits 0-3, ODR1 for bits 4-7) that are accessed as a single 16-bit value.
 *
 * @par ODR Register Layout
 * @code
 * ODR (16-bit):
 * [15:12] Reserved
 * [11:8]  ODR1 - Open drain for pins Pn4-Pn7 (B7-B4 bits)
 * [7:4]   Reserved
 * [3:0]   ODR0 - Open drain for pins Pn0-Pn3 (B3-B0 bits)
 * @endcode
 *
 * @par Inter-Port ODR Spacing
 * ODR registers are spaced 2 bytes apart (16-bit access):
 * - PORT0.ODR: 0x0008C080
 * - PORT1.ODR: 0x0008C082
 * - PORT2.ODR: 0x0008C084
 * - etc.
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies ODR size is 2 bytes and inter-port spacing is 2 bytes
 *
 * @since Version 1.0.0
 */
void test_odr_layout(void)
{
  /* ODR is 16-bit register accessed via dedicated accessor functions */
  TEST_ASSERT_EQUAL_size_t(2, sizeof(*port0_odr()));

  /* Verify ODR address spacing between ports (word addressing) */
  /* PORT0.ODR at 0x0008C080, PORT1.ODR at 0x0008C082 (2-byte spacing) */
  uintptr_t odr0_addr = (uintptr_t)port0_odr();
  uintptr_t odr1_addr = (uintptr_t)port1_odr();

  TEST_ASSERT_EQUAL_HEX32(0x0008C080, odr0_addr);     /* PORT0.ODR */
  TEST_ASSERT_EQUAL_HEX32(0x0008C082, odr1_addr);     /* PORT1.ODR */
  TEST_ASSERT_EQUAL_UINT32(2, odr1_addr - odr0_addr); /* 2-byte spacing */
}

/* =============================================================================
 * Register Field Width Tests
 * =============================================================================
 */

/**
 * @brief Verify all register field widths are correct (8-bit except ODR)
 *
 * @details
 * Most port registers are 8-bit (one bit per pin on the port). The exception
 * is ODR which is 16-bit (combines ODR0 and ODR1 for 8-bit port access).
 *
 * @par Expected Field Sizes
 * | Register | Size   | Access | Notes                         |
 * |----------|--------|--------|-------------------------------|
 * | PDR      | 8-bit  | R/W    | 1 bit per pin (direction)     |
 * | PODR     | 8-bit  | R/W    | 1 bit per pin (output data)   |
 * | PIDR     | 8-bit  | R      | 1 bit per pin (input data)    |
 * | PMR      | 8-bit  | R/W    | 1 bit per pin (peripheral)    |
 * | ODR      | 16-bit | R/W    | 2 bits per pin (open drain)   |
 * | PCR      | 8-bit  | R/W    | 1 bit per pin (pull-up)       |
 * | DSCR     | 8-bit  | R/W    | 1 bit per pin (drive strength)|
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies sizeof() for each register field in rx_port_regs_t
 *
 * @since Version 1.0.0
 */
void test_register_field_widths(void)
{
  volatile rx_port_regs_t* p = port0();

  /* All single registers should be 8-bit (1 byte) */
  TEST_ASSERT_EQUAL_size_t(1, sizeof(p->pdr));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(p->podr));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(p->pidr));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(p->pmr));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(p->pcr));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(p->dscr));

  /* ODR should be 16-bit (2 bytes) - accessed via port*_odr() functions */
  TEST_ASSERT_EQUAL_size_t(2, sizeof(*port0_odr()));
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner main function
 *
 * @details
 * Executes all GPIO register verification tests in the following groups:
 * 1. Register Base Address Tests (7 assertions)
 * 2. Port Offset Tests (12 assertions)
 * 3. Struct Layout Tests (8 assertions)
 * 4. Accessor Function Tests (9 test functions, ~45 assertions)
 * 5. ODR Register Tests (4 assertions)
 * 6. Register Field Width Tests (7 assertions)
 *
 * @return int 0 on all tests pass, non-zero on failure
 *
 * @par Exit Codes
 * - 0: All tests passed
 * - Non-zero: Number of failed tests
 *
 * @note Run via: `ctest --test-dir build -V` from tests/ directory
 *
 * @since Version 1.0.0
 */
int main(void)
{
  UNITY_BEGIN();

  /* Register Base Address Tests */
  RUN_TEST(test_port_base_addresses);

  /* Port Offset Tests */
  RUN_TEST(test_port_offsets);

  /* Struct Layout Tests */
  RUN_TEST(test_port_struct_offsets);
  RUN_TEST(test_port_struct_size);

  /* Accessor Function Tests */
  RUN_TEST(test_port0_accessor);
  RUN_TEST(test_port1_accessor);
  RUN_TEST(test_port2_accessor);
  RUN_TEST(test_porta_accessor);
  RUN_TEST(test_portb_accessor);
  RUN_TEST(test_portc_accessor);
  RUN_TEST(test_portd_accessor);
  RUN_TEST(test_porte_accessor);
  RUN_TEST(test_portj_accessor);

  /* ODR Register Tests */
  RUN_TEST(test_odr_layout);

  /* Register Field Width Tests */
  RUN_TEST(test_register_field_widths);

  return UNITY_END();
}
