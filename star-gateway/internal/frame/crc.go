// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package frame provides CRC-32 implementation.
package frame

import "hash/crc32"

// computeCRC32 calculates the CRC-32 checksum (IEEE 802.3).
func computeCRC32(data []byte) uint32 {
	return crc32.ChecksumIEEE(data)
}

// verifyCRC32 verifies if the data matches the received checksum.
func verifyCRC32(data []byte, received uint32) bool {
	return computeCRC32(data) == received
}
