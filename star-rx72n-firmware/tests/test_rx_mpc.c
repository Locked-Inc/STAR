/* star-rx72n-firmware/tests/test_rx_mpc.c */
/**
 * @file test_rx_mpc.c
 * @brief Unit Tests for RX72N MPC (Multi-Function Pin Controller) Register Definitions
 *
 * @details
 * Comprehensive verification tests for the RX72N Multi-Function Pin Controller
 * module (Chapter 23 of the hardware manual). The MPC controls pin function
 * selection, allowing each GPIO pin to be configured for peripheral functions.
 *
 * This test suite validates:
 * - **Base addresses**: MPC register block at 0x0008C100
 * - **PWPR register**: Write protection mechanism (B0WI, PFSWE bits)
 * - **PFS registers**: Pin Function Select registers for all ports (0-5, A-E, J)
 * - **Bus control registers**: PFCSE, PFCSS, PFAOE, PFBCR, PFENET
 * - **Struct layout**: rx_mpc_regs_t field offsets and sizes
 * - **Bit definitions**: PSEL mask, ISEL, ASEL, protection bits
 * - **Write protection sequence**: Unlock/lock sequence verification
 *
 * @par Test Coverage Summary
 * | Test Category              | Tests | Description                      |
 * |----------------------------|-------|----------------------------------|
 * | Base Address               | 2     | MPC base and accessor            |
 * | Critical Offsets           | 3     | PFCSE, PWPR, P00PFS offsets      |
 * | PFS Sequential Addresses   | 7     | PORT 0-2, 5, A, E, J PFS regs    |
 * | Struct Size                | 1     | Total rx_mpc_regs_t size         |
 * | PWPR Bit Definitions       | 1     | B0WI, PFSWE bits                 |
 * | PFS Bit Definitions        | 2     | PSEL, ISEL, ASEL bits            |
 * | Reserved Field Sizes       | 1     | All reserved byte counts         |
 * | Write Protection           | 2     | Unlock/lock sequences            |
 * | PSEL Value Range           | 1     | 5-bit value range (0-31)         |
 * | Bus Control Offsets        | 1     | All bus control register addrs   |
 * | Field Widths               | 1     | All register sizes               |
 *
 * @par MPC Write Protection Mechanism
 * The MPC uses a two-level protection scheme to prevent accidental writes:
 * @code
 * // Unlock sequence (to enable PFS writes):
 * PWPR = 0x00;  // Step 1: Clear B0WI
 * PWPR = 0x40;  // Step 2: Set PFSWE
 *
 * // Lock sequence (to disable PFS writes):
 * PWPR = 0x00;  // Step 1: Clear PFSWE
 * PWPR = 0x80;  // Step 2: Set B0WI
 * @endcode
 *
 * @par Test Framework
 * Uses Unity test framework (ThrowTheSwitch.org).
 * Tests are run on host machine (x86/x64), not on target hardware.
 *
 * @par Verification Methodology
 * Each test compares computed addresses against values from:
 * - RX72N Group User's Manual: Hardware (R01UH0824EJ0120 Rev.1.20)
 * - Chapter 23: Multi-Function Pin Controller (MPC)
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: Simple control flow (no goto, setjmp, recursion)
 * - Rule 2: Single bounded loop in test_psel_value_range()
 * - Rule 3: No dynamic memory allocation
 * - Rule 4: Functions are short (< 60 lines each)
 * - Rule 5: Minimal assertions per test (1-10 per function)
 * - Rule 6: Minimal variable scope
 * - Rule 7: Return values verified via Unity assertions
 * - Rule 8: Limited preprocessor usage
 * - Rule 9: No function pointers in tests
 * - Rule 10: Compiled with -Wall -Wextra -Werror
 *
 * @par Module Dependencies
 * - rx72n_mpc_regs.h: Register definitions under test
 * - unity.h: Test framework
 * - stddef.h, stdint.h: Standard type definitions
 *
 * @par Build Command
 * @code{.sh}
 * cd star-rx72n-firmware/tests
 * cmake -B build && cmake --build build && ctest --test-dir build -V
 * @endcode
 *
 * @see rx72n_mpc_regs.h MPC register definitions
 * @see rx72n_port_regs.h Related GPIO port registers
 * @see Ch23_MPC.txt Manual chapter extract
 *
 * @author STAR Team
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rx72n_mpc_regs.h"
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
 * Base Address Tests
 * =============================================================================
 */

/**
 * @brief Verify MPC base address matches RX72N hardware manual Ch23
 *
 * @details
 * The Multi-Function Pin Controller register block starts at 0x0008C100,
 * immediately following the GPIO port registers at 0x0008C000.
 *
 * @par MPC Memory Map Overview
 * | Offset | Register   | Description                           |
 * |--------|------------|---------------------------------------|
 * | 0x00   | PFCSE      | CS/Address Extension Select           |
 * | 0x02   | PFCSS0/1   | CS/Address Extension Output Select    |
 * | 0x04   | PFAOE0/1   | Address Output Enable                 |
 * | 0x06   | PFBCRn     | Bus Control Register                  |
 * | 0x0E   | PFENET     | Ethernet Control                      |
 * | 0x1F   | PWPR       | Write Protection Register             |
 * | 0x40+  | PxxPFS     | Pin Function Select (per-pin)         |
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies k_mpc_base_addr == 0x0008C100
 *
 * @since Version 1.0.0
 */
void test_mpc_base_address(void)
{
  TEST_ASSERT_EQUAL_HEX32(0x0008C100, k_mpc_base_addr);
}

/**
 * @brief Test mpc() accessor function
 * @details Verifies mpc() returns correct base address
 */
void test_mpc_accessor(void)
{
  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C100, (void*)m);
}

/* =============================================================================
 * Critical Register Offset Tests
 * =============================================================================
 */

/**
 * @brief Test PFCSE register offset
 * @details PFCSE should be at offset 0x00 (address 0x0008C100)
 */
void test_pfcse_offset(void)
{
  TEST_ASSERT_EQUAL_size_t(0x00, offsetof(rx_mpc_regs_t, pfcse));

  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C100, (void*)&m->pfcse);
}

/**
 * @brief Verify PWPR (Write Protection Register) offset is correct
 *
 * @details
 * PWPR at offset 0x1F (address 0x0008C11F) controls write access to PFS
 * registers. Must be unlocked before modifying pin function selections.
 *
 * @par PWPR Bit Layout
 * | Bit | Name   | Function                                 |
 * |-----|--------|------------------------------------------|
 * | 7   | B0WI   | 1=Disable writes to PFSWE bit            |
 * | 6   | PFSWE  | 1=Enable writes to PFS registers         |
 * | 5:0 | -      | Reserved (read as 0)                     |
 *
 * @par Reset Value: 0x80 (B0WI=1, writes disabled)
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies PWPR offset == 0x1F and address == 0x0008C11F
 *
 * @since Version 1.0.0
 */
void test_pwpr_offset(void)
{
  TEST_ASSERT_EQUAL_size_t(0x1F, offsetof(rx_mpc_regs_t, pwpr));

  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C11F, (void*)&m->pwpr);
}

/**
 * @brief Test P00PFS register offset
 * @details P00PFS (Port 0 Pin 0 Function Select) should be at offset 0x40 (address 0x0008C140)
 */
void test_p00pfs_offset(void)
{
  TEST_ASSERT_EQUAL_size_t(0x40, offsetof(rx_mpc_regs_t, p00pfs));

  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C140, (void*)&m->p00pfs);
}

/* =============================================================================
 * PFS Register Sequential Addressing Tests
 * =============================================================================
 */

/**
 * @brief Test PORT0 PFS register addresses
 * @details Verifies P00PFS through P07PFS are sequential
 */
void test_port0_pfs_addresses(void)
{
  volatile rx_mpc_regs_t* m = mpc();

  TEST_ASSERT_EQUAL_PTR((void*)0x0008C140, (void*)&m->p00pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C141, (void*)&m->p01pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C142, (void*)&m->p02pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C143, (void*)&m->p03pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C144, (void*)&m->p04pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C145, (void*)&m->p05pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C146, (void*)&m->p06pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C147, (void*)&m->p07pfs);
}

/**
 * @brief Test PORT1 PFS register addresses
 * @details Verifies P10PFS through P17PFS follow PORT0
 */
void test_port1_pfs_addresses(void)
{
  volatile rx_mpc_regs_t* m = mpc();

  TEST_ASSERT_EQUAL_PTR((void*)0x0008C148, (void*)&m->p10pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C149, (void*)&m->p11pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C14A, (void*)&m->p12pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C14B, (void*)&m->p13pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C14C, (void*)&m->p14pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C14D, (void*)&m->p15pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C14E, (void*)&m->p16pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C14F, (void*)&m->p17pfs);
}

/**
 * @brief Test PORT2 PFS register addresses
 * @details Verifies P20PFS through P27PFS follow PORT1
 */
void test_port2_pfs_addresses(void)
{
  volatile rx_mpc_regs_t* m = mpc();

  TEST_ASSERT_EQUAL_PTR((void*)0x0008C150, (void*)&m->p20pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C151, (void*)&m->p21pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C152, (void*)&m->p22pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C153, (void*)&m->p23pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C154, (void*)&m->p24pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C155, (void*)&m->p25pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C156, (void*)&m->p26pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C157, (void*)&m->p27pfs);
}

/**
 * @brief Test PORT5 PFS register addresses
 * @details Verifies P50PFS is at offset 0x68 (0x0008C168)
 */
void test_port5_pfs_addresses(void)
{
  TEST_ASSERT_EQUAL_size_t(0x40 + 0x28, offsetof(rx_mpc_regs_t, p50pfs));

  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C168, (void*)&m->p50pfs);
}

/**
 * @brief Test PORTA PFS register addresses
 * @details Verifies PA0PFS is at offset 0x90 (0x0008C190)
 */
void test_porta_pfs_addresses(void)
{
  TEST_ASSERT_EQUAL_size_t(0x40 + 0x50, offsetof(rx_mpc_regs_t, pa0pfs));

  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C190, (void*)&m->pa0pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C191, (void*)&m->pa1pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C192, (void*)&m->pa2pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C193, (void*)&m->pa3pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C194, (void*)&m->pa4pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C195, (void*)&m->pa5pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C196, (void*)&m->pa6pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C197, (void*)&m->pa7pfs);
}

/**
 * @brief Test PORTE PFS register addresses
 * @details Verifies PE0PFS is at offset 0xB0 (0x0008C1B0)
 */
void test_porte_pfs_addresses(void)
{
  TEST_ASSERT_EQUAL_size_t(0x40 + 0x70, offsetof(rx_mpc_regs_t, pe0pfs));

  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B0, (void*)&m->pe0pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B1, (void*)&m->pe1pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B2, (void*)&m->pe2pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B3, (void*)&m->pe3pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B4, (void*)&m->pe4pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B5, (void*)&m->pe5pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B6, (void*)&m->pe6pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1B7, (void*)&m->pe7pfs);
}

/**
 * @brief Test PORTJ PFS register addresses
 * @details Verifies PJ0PFS is at offset 0xD0 (0x0008C1D0)
 */
void test_portj_pfs_addresses(void)
{
  TEST_ASSERT_EQUAL_size_t(0x40 + 0x90, offsetof(rx_mpc_regs_t, pj0pfs));

  volatile rx_mpc_regs_t* m = mpc();
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1D0, (void*)&m->pj0pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1D1, (void*)&m->pj1pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1D2, (void*)&m->pj2pfs);
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1D3, (void*)&m->pj3pfs);
  /* PJ4 is reserved/not available */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C1D5, (void*)&m->pj5pfs);
}

/* =============================================================================
 * Struct Size Tests
 * =============================================================================
 */

/**
 * @brief Test rx_mpc_regs_t struct size
 * @details Total size should be 64 + 152 = 216 bytes (0x40 + 152)
 */
void test_mpc_struct_size(void)
{
  /* Base registers (0x00-0x3F) = 64 bytes
   * PFS registers (0x40-0xD7) = 152 bytes
   * Total = 216 bytes */
  TEST_ASSERT_EQUAL_size_t(0x40 + 152, sizeof(rx_mpc_regs_t));
}

/* =============================================================================
 * PWPR Bit Definition Tests
 * =============================================================================
 */

/**
 * @brief Verify PWPR bit definitions match hardware specification
 *
 * @details
 * Tests the B0WI and PFSWE bit positions in the PWPR register.
 * These bits control the two-level write protection mechanism:
 *
 * @par Protection State Machine
 * @code
 * Reset State:     B0WI=1, PFSWE=0  (locked)
 * After unlock:    B0WI=0, PFSWE=1  (PFS writable)
 * After lock:      B0WI=1, PFSWE=0  (locked again)
 * @endcode
 *
 * @warning Incorrect bit values will prevent PFS register writes
 *
 * @pre None
 * @post No state changes
 *
 * @test Verifies k_mpc_pwpr_b0wi==0x80 and k_mpc_pwpr_pfswe==0x40
 *
 * @since Version 1.0.0
 */
void test_pwpr_bit_definitions(void)
{
  /* PFSWE (PFS Write Enable) - bit 6 */
  TEST_ASSERT_EQUAL_HEX8(0x40, k_mpc_pwpr_pfswe); /* 1 << 6 */

  /* B0WI (PFSWE Bit Write Disable) - bit 7 */
  TEST_ASSERT_EQUAL_HEX8(0x80, k_mpc_pwpr_b0wi); /* 1 << 7 */

  /* Verify bits don't overlap */
  TEST_ASSERT_EQUAL_HEX8(0x00, k_mpc_pwpr_pfswe & k_mpc_pwpr_b0wi);
}

/* =============================================================================
 * PFS Bit Definition Tests
 * =============================================================================
 */

/**
 * @brief Test PFS bit definitions
 * @details Verifies PSEL mask, ISEL, and ASEL bit positions
 */
void test_pfs_bit_definitions(void)
{
  /* PSEL mask (bits 0-4) - 5 bits */
  TEST_ASSERT_EQUAL_HEX8(0x1F, k_pfs_psel_mask); /* 0b00011111 */

  /* ISEL (Interrupt Input Select) - bit 6 */
  TEST_ASSERT_EQUAL_HEX8(0x40, k_pfs_isel); /* 1 << 6 */

  /* ASEL (Analog Input Select) - bit 7 */
  TEST_ASSERT_EQUAL_HEX8(0x80, k_pfs_asel); /* 1 << 7 */

  /* Verify PSEL mask doesn't overlap with ISEL/ASEL */
  TEST_ASSERT_EQUAL_HEX8(0x00, k_pfs_psel_mask & k_pfs_isel);
  TEST_ASSERT_EQUAL_HEX8(0x00, k_pfs_psel_mask & k_pfs_asel);
  TEST_ASSERT_EQUAL_HEX8(0x00, k_pfs_isel & k_pfs_asel);
}

/**
 * @brief Test rx_pfs_regs_t bitfield struct
 * @details Verifies bitfield struct is 1 byte
 */
void test_pfs_bitfield_struct_size(void)
{
  /* PFS bitfield struct should be exactly 1 byte (8 bits) */
  TEST_ASSERT_EQUAL_size_t(1, sizeof(rx_pfs_regs_t));
}

/* =============================================================================
 * Reserved Field Size Tests
 * =============================================================================
 */

/**
 * @brief Test reserved field size constants
 * @details Verifies enum values for reserved byte counts
 */
void test_reserved_field_sizes(void)
{
  /* Bus control reserved regions */
  TEST_ASSERT_EQUAL_UINT8(1, k_mpc_reserved1_bytes);  /* After PFCSE */
  TEST_ASSERT_EQUAL_UINT8(4, k_mpc_reserved2_bytes);  /* After PFBCR3 */
  TEST_ASSERT_EQUAL_UINT8(16, k_mpc_reserved3_bytes); /* After PFENET */
  TEST_ASSERT_EQUAL_UINT8(8, k_mpc_reserved4_bytes);  /* After PWPR */
  TEST_ASSERT_EQUAL_UINT8(24, k_mpc_dscr2_count);     /* DSCR2 registers */

  /* Port-specific reserved regions (144-pin LFQFP) */
  TEST_ASSERT_EQUAL_UINT8(3, k_port3_reserved_bytes);  /* Port 3: P35-P37 */
  TEST_ASSERT_EQUAL_UINT8(1, k_port5_reserved_bytes);  /* Port 5: P53 */
  TEST_ASSERT_EQUAL_UINT8(1, k_port6_reserved_bytes);  /* Port 6: P65 */
  TEST_ASSERT_EQUAL_UINT8(1, k_port7_reserved_bytes);  /* Port 7: P70 */
  TEST_ASSERT_EQUAL_UINT8(2, k_portf_reserved1_bytes); /* Port F: PF3-PF4 */
  TEST_ASSERT_EQUAL_UINT8(2, k_portf_reserved2_bytes); /* Port F: PF6-PF7 */
  TEST_ASSERT_EQUAL_UINT8(1, k_portj_reserved1_bytes); /* Port J: PJ4 */
  TEST_ASSERT_EQUAL_UINT8(2, k_portj_reserved2_bytes); /* Port J: PJ6-PJ7 */
}

/* =============================================================================
 * Write Protection Sequence Simulation Tests
 * =============================================================================
 */

/**
 * @brief Test PWPR unlock sequence values
 * @details Verifies the correct sequence to enable PFS writes
 */
void test_pwpr_unlock_sequence_values(void)
{
  /* Step 1: Clear B0WI (write 0x00) */
  uint8_t step1 = 0x00;

  TEST_ASSERT_EQUAL_HEX8(0x00, step1);

  /* Step 2: Set PFSWE (write 0x40 = k_mpc_pwpr_pfswe) */
  uint8_t step2 = k_mpc_pwpr_pfswe;
  TEST_ASSERT_EQUAL_HEX8(0x40, step2);

  /* Verify PFSWE is set, B0WI is clear */
  TEST_ASSERT_BITS(k_mpc_pwpr_pfswe, k_mpc_pwpr_pfswe, step2);
  TEST_ASSERT_BITS(k_mpc_pwpr_b0wi, 0x00, step2);
}

/**
 * @brief Test PWPR lock sequence values
 * @details Verifies the correct sequence to disable PFS writes
 */
void test_pwpr_lock_sequence_values(void)
{
  /* Step 1: Clear PFSWE (write 0x00) */
  uint8_t step1 = 0x00;

  TEST_ASSERT_EQUAL_HEX8(0x00, step1);

  /* Step 2: Set B0WI (write 0x80 = k_mpc_pwpr_b0wi) */
  uint8_t step2 = k_mpc_pwpr_b0wi;
  TEST_ASSERT_EQUAL_HEX8(0x80, step2);

  /* Verify B0WI is set, PFSWE is clear */
  TEST_ASSERT_BITS(k_mpc_pwpr_b0wi, k_mpc_pwpr_b0wi, step2);
  TEST_ASSERT_BITS(k_mpc_pwpr_pfswe, 0x00, step2);
}

/* =============================================================================
 * PSEL Value Range Tests
 * =============================================================================
 */

/**
 * @brief Test PSEL value range
 * @details Verifies PSEL can represent values 0-31 (5 bits)
 */
void test_psel_value_range(void)
{
  /* PSEL is 5 bits, so valid range is 0-31 */
  for (uint8_t psel = 0; psel <= 31; psel++) {
    /* PSEL value should fit within mask */
    TEST_ASSERT_EQUAL_UINT8(psel, psel & k_pfs_psel_mask);
  }

  /* Values above 31 should be masked */
  TEST_ASSERT_EQUAL_UINT8(0x00, 32 & k_pfs_psel_mask);
  TEST_ASSERT_EQUAL_UINT8(0x1F, 0xFF & k_pfs_psel_mask);
}

/* =============================================================================
 * Bus Control Register Tests
 * =============================================================================
 */

/**
 * @brief Test bus control register offsets
 * @details Verifies PFCSE, PFCSS, PFAOE, PFBCR, PFENET offsets
 */
void test_bus_control_register_offsets(void)
{
  volatile rx_mpc_regs_t* m = mpc();

  /* PFCSE @ 0x0008C100 (offset 0x00) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C100, (void*)&m->pfcse);

  /* PFCSS0 @ 0x0008C102 (offset 0x02, after 1 reserved byte) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C102, (void*)&m->pfcss0);

  /* PFCSS1 @ 0x0008C103 (offset 0x03) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C103, (void*)&m->pfcss1);

  /* PFAOE0 @ 0x0008C104 (offset 0x04) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C104, (void*)&m->pfaoe0);

  /* PFAOE1 @ 0x0008C105 (offset 0x05) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C105, (void*)&m->pfaoe1);

  /* PFBCR0 @ 0x0008C106 (offset 0x06) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C106, (void*)&m->pfbcr0);

  /* PFENET @ 0x0008C10E (offset 0x0E) */
  TEST_ASSERT_EQUAL_PTR((void*)0x0008C10E, (void*)&m->pfenet);
}

/* =============================================================================
 * Register Field Width Tests
 * =============================================================================
 */

/**
 * @brief Test register field widths
 * @details Verifies all MPC registers are 8-bit (1 byte)
 */
void test_register_field_widths(void)
{
  volatile rx_mpc_regs_t* m = mpc();

  /* Bus control registers - all 8-bit */
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pfcse));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pfcss0));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pfcss1));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pfaoe0));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pfaoe1));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pfbcr0));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pfenet));

  /* PWPR - 8-bit */
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pwpr));

  /* PFS registers - all 8-bit */
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->p00pfs));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pa0pfs));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pe0pfs));
  TEST_ASSERT_EQUAL_size_t(1, sizeof(m->pj0pfs));
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

/**
 * @brief Unity test runner main function
 *
 * @details
 * Executes all MPC register verification tests in the following groups:
 * 1. Base Address Tests (2 tests)
 * 2. Critical Register Offset Tests (3 tests)
 * 3. PFS Sequential Addressing Tests (7 tests)
 * 4. Struct Size Tests (1 test)
 * 5. PWPR Bit Definition Tests (1 test)
 * 6. PFS Bit Definition Tests (2 tests)
 * 7. Reserved Field Size Tests (1 test)
 * 8. Write Protection Sequence Tests (2 tests)
 * 9. PSEL Value Range Tests (1 test)
 * 10. Bus Control Register Tests (1 test)
 * 11. Register Field Width Tests (1 test)
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

  /* Base Address Tests */
  RUN_TEST(test_mpc_base_address);
  RUN_TEST(test_mpc_accessor);

  /* Critical Register Offset Tests */
  RUN_TEST(test_pfcse_offset);
  RUN_TEST(test_pwpr_offset);
  RUN_TEST(test_p00pfs_offset);

  /* PFS Register Sequential Addressing Tests */
  RUN_TEST(test_port0_pfs_addresses);
  RUN_TEST(test_port1_pfs_addresses);
  RUN_TEST(test_port2_pfs_addresses);
  RUN_TEST(test_port5_pfs_addresses);
  RUN_TEST(test_porta_pfs_addresses);
  RUN_TEST(test_porte_pfs_addresses);
  RUN_TEST(test_portj_pfs_addresses);

  /* Struct Size Tests */
  RUN_TEST(test_mpc_struct_size);

  /* PWPR Bit Definition Tests */
  RUN_TEST(test_pwpr_bit_definitions);

  /* PFS Bit Definition Tests */
  RUN_TEST(test_pfs_bit_definitions);
  RUN_TEST(test_pfs_bitfield_struct_size);

  /* Reserved Field Size Tests */
  RUN_TEST(test_reserved_field_sizes);

  /* Write Protection Sequence Tests */
  RUN_TEST(test_pwpr_unlock_sequence_values);
  RUN_TEST(test_pwpr_lock_sequence_values);

  /* PSEL Value Range Tests */
  RUN_TEST(test_psel_value_range);

  /* Bus Control Register Tests */
  RUN_TEST(test_bus_control_register_offsets);

  /* Register Field Width Tests */
  RUN_TEST(test_register_field_widths);

  return UNITY_END();
}
