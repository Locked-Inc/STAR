/**
 * @file crc8.h
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* crc8.h -- Dallas/Maxim CRC-8 (poly 0x07, init 0x00, no reflection, no xor-out).
 *
 * Single function, lookup-table based. Same poly used by ATM HEC and many
 * trivial wire protocols. NOT the SMBus CRC-8 (which uses init 0xFF).
 *
 * Why CRC-8 and not CRC-16/32: payloads are tiny (max 32 bytes). CRC-8 catches
 * every single-bit error in a 32-byte frame and ~99.6% of multi-bit errors,
 * which is more than enough on a 1 m USB tether between the RX72N and the Pi5.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/* Compute CRC-8 over `data[0..len)` with poly 0x07, init 0x00. */
uint8_t crc8(const uint8_t *data, size_t len);
