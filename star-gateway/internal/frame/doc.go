// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

// Package frame defines the wire protocol frame structure and decoding logic.
//
// The protocol uses a custom framing format:
// [SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)][PAYLOAD(0-1KB)][CRC-32(4)]
//
// Key features:
// - 2-byte sync word for frame alignment (0x55AA)
// - 16-bit sequence number
// - CRC-32 (IEEE 802.3) data integrity
// - Max payload 1024 bytes
// - Type field for message classification
//
// STAR Project - Texas A&M University
package frame
