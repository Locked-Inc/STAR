// Frame protocol implementation matching RX72N rx_usb_comm
// STAR Project - Texas A&M University
//
// Frame format: [SYNC(0x55AA) | SEQ | LEN | TYPE | FLAGS | PAYLOAD | CRC-32]

use crc32fast::Hasher;
use thiserror::Error;

// Frame protocol constants
pub const FRAME_SYNC_BYTE_1: u8 = 0x55;
pub const FRAME_SYNC_BYTE_2: u8 = 0xAA;
pub const FRAME_HEADER_SIZE: usize = 8; // SYNC(2) + SEQ(1) + LEN(2) + TYPE(1) + FLAGS(2)
pub const FRAME_FOOTER_SIZE: usize = 4; // CRC-32
pub const FRAME_MAX_PAYLOAD: usize = 1024;

pub const FRAME_TYPE_REQUEST: u8 = 0x01;
pub const FRAME_TYPE_RESPONSE: u8 = 0x02;

#[derive(Error, Debug)]
pub enum FrameError {
    #[error("Payload too large: {0} > {1}")]
    PayloadTooLarge(usize, usize),

    #[error("Frame too short: {0} bytes")]
    FrameTooShort(usize),

    #[error("Invalid sync bytes: 0x{0:02X}{1:02X}")]
    InvalidSyncBytes(u8, u8),

    #[error("Incomplete frame: got {got} bytes, expected {expected}")]
    IncompleteFrame { got: usize, expected: usize },

    #[error("CRC mismatch: got 0x{got:08X}, expected 0x{expected:08X}")]
    CrcMismatch { got: u32, expected: u32 },
}

/// Represents a framed message
#[derive(Debug, Clone)]
pub struct Frame {
    pub sequence: u8,
    pub length: u16,
    pub frame_type: u8,
    pub flags: u16,
    pub payload: Vec<u8>,
    pub crc32: u32,
}

impl Frame {
    /// Encodes a frame for transmission
    pub fn encode(
        sequence: u8,
        frame_type: u8,
        flags: u16,
        payload: &[u8],
    ) -> Result<Vec<u8>, FrameError> {
        if payload.len() > FRAME_MAX_PAYLOAD {
            return Err(FrameError::PayloadTooLarge(
                payload.len(),
                FRAME_MAX_PAYLOAD,
            ));
        }

        let payload_len = payload.len() as u16;
        let total_len = FRAME_HEADER_SIZE + payload.len() + FRAME_FOOTER_SIZE;

        let mut buf = vec![0u8; total_len];
        let mut pos = 0;

        // Sync bytes (0x55AA)
        buf[pos] = FRAME_SYNC_BYTE_1;
        pos += 1;
        buf[pos] = FRAME_SYNC_BYTE_2;
        pos += 1;

        // Sequence number
        buf[pos] = sequence;
        pos += 1;

        // Payload length (16-bit little-endian)
        buf[pos..pos + 2].copy_from_slice(&payload_len.to_le_bytes());
        pos += 2;

        // Frame type
        buf[pos] = frame_type;
        pos += 1;

        // Flags (16-bit little-endian)
        buf[pos..pos + 2].copy_from_slice(&flags.to_le_bytes());
        pos += 2;

        // Payload
        buf[pos..pos + payload.len()].copy_from_slice(payload);
        pos += payload.len();

        // CRC-32 (IEEE 802.3 polynomial, little-endian)
        // CRC is calculated over entire frame except CRC field itself
        let mut hasher = Hasher::new();
        hasher.update(&buf[..pos]);
        let crc = hasher.finalize();
        buf[pos..pos + 4].copy_from_slice(&crc.to_le_bytes());

        Ok(buf)
    }

    /// Decodes a received frame
    pub fn decode(data: &[u8]) -> Result<Self, FrameError> {
        if data.len() < FRAME_HEADER_SIZE + FRAME_FOOTER_SIZE {
            return Err(FrameError::FrameTooShort(data.len()));
        }

        let mut pos = 0;

        // Check sync bytes
        if data[pos] != FRAME_SYNC_BYTE_1 || data[pos + 1] != FRAME_SYNC_BYTE_2 {
            return Err(FrameError::InvalidSyncBytes(data[pos], data[pos + 1]));
        }
        pos += 2;

        // Parse header
        let sequence = data[pos];
        pos += 1;

        let length = u16::from_le_bytes([data[pos], data[pos + 1]]);
        pos += 2;

        let frame_type = data[pos];
        pos += 1;

        let flags = u16::from_le_bytes([data[pos], data[pos + 1]]);
        pos += 2;

        // Verify total length
        let expected_len = FRAME_HEADER_SIZE + length as usize + FRAME_FOOTER_SIZE;
        if data.len() < expected_len {
            return Err(FrameError::IncompleteFrame {
                got: data.len(),
                expected: expected_len,
            });
        }

        // Extract payload
        let payload = if length > 0 {
            data[pos..pos + length as usize].to_vec()
        } else {
            Vec::new()
        };
        pos += length as usize;

        // Extract and verify CRC-32
        let crc32 = u32::from_le_bytes([data[pos], data[pos + 1], data[pos + 2], data[pos + 3]]);
        pos += 4;

        // Calculate expected CRC (over everything except CRC field)
        let mut hasher = Hasher::new();
        hasher.update(&data[..pos - 4]);
        let expected_crc = hasher.finalize();

        if crc32 != expected_crc {
            return Err(FrameError::CrcMismatch {
                got: crc32,
                expected: expected_crc,
            });
        }

        Ok(Frame {
            sequence,
            length,
            frame_type,
            flags,
            payload,
            crc32,
        })
    }
}

/// Scans data for frame sync bytes and returns the offset
/// Returns None if no sync found
pub fn find_frame_sync(data: &[u8]) -> Option<usize> {
    (0..data.len().saturating_sub(1))
        .find(|&i| data[i] == FRAME_SYNC_BYTE_1 && data[i + 1] == FRAME_SYNC_BYTE_2)
}

/// Attempts to read a complete frame from a buffer
/// Returns: (frame, bytes_consumed, error)
/// If no complete frame is available, returns (None, 0, None)
pub fn read_frame_from_buffer(buf: &[u8]) -> (Option<Frame>, usize, Option<FrameError>) {
    // Need at least header + footer
    if buf.len() < FRAME_HEADER_SIZE + FRAME_FOOTER_SIZE {
        return (None, 0, None); // Need more data
    }

    // Find sync bytes
    let sync_offset = match find_frame_sync(buf) {
        Some(offset) => offset,
        None => {
            // No sync found, discard all but last byte (might be start of sync)
            let discard = if buf.len() > 1 { buf.len() - 1 } else { 0 };
            return (None, discard, None);
        }
    };

    // Use buffer from sync point
    let buf = &buf[sync_offset..];

    // Check if we have enough data for header
    if buf.len() < FRAME_HEADER_SIZE {
        return (None, sync_offset, None); // Need more data
    }

    // Parse length field to know total frame size
    let payload_len = u16::from_le_bytes([buf[3], buf[4]]) as usize;
    let total_frame_len = FRAME_HEADER_SIZE + payload_len + FRAME_FOOTER_SIZE;

    // Check if we have complete frame
    if buf.len() < total_frame_len {
        return (None, sync_offset, None); // Need more data
    }

    // Decode complete frame
    match Frame::decode(&buf[..total_frame_len]) {
        Ok(frame) => (Some(frame), sync_offset + total_frame_len, None),
        Err(err) => {
            // Frame decode failed, skip this sync and try next
            (None, sync_offset + 2, Some(err))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_decode() {
        let payload = b"Hello, BMS!";
        let sequence = 42;
        let flags = 0x0000;

        let encoded = Frame::encode(sequence, FRAME_TYPE_REQUEST, flags, payload).unwrap();
        let decoded = Frame::decode(&encoded).unwrap();

        assert_eq!(decoded.sequence, sequence);
        assert_eq!(decoded.frame_type, FRAME_TYPE_REQUEST);
        assert_eq!(decoded.flags, flags);
        assert_eq!(decoded.payload, payload);
    }

    #[test]
    fn test_find_sync() {
        let data = vec![0x00, 0x11, 0x55, 0xAA, 0x42];
        assert_eq!(find_frame_sync(&data), Some(2));

        let data_no_sync = vec![0x00, 0x11, 0x22, 0x33];
        assert_eq!(find_frame_sync(&data_no_sync), None);
    }

    #[test]
    fn test_payload_too_large() {
        let large_payload = vec![0u8; FRAME_MAX_PAYLOAD + 1];
        let result = Frame::encode(0, FRAME_TYPE_REQUEST, 0, &large_payload);
        assert!(matches!(result, Err(FrameError::PayloadTooLarge(_, _))));
    }

    #[test]
    fn test_crc_validation() {
        let payload = b"Test data";
        let mut encoded = Frame::encode(1, FRAME_TYPE_RESPONSE, 0, payload).unwrap();

        // Corrupt the CRC (last 4 bytes)
        let len = encoded.len();
        encoded[len - 1] ^= 0xFF;

        let result = Frame::decode(&encoded);
        assert!(matches!(result, Err(FrameError::CrcMismatch { .. })));
    }

    #[test]
    fn test_truncated_frame() {
        let payload = b"Data";
        let encoded = Frame::encode(1, FRAME_TYPE_REQUEST, 0, payload).unwrap();

        // Try to decode with truncated data
        let truncated = &encoded[..encoded.len() - 5];
        let result = Frame::decode(truncated);
        assert!(matches!(result, Err(FrameError::FrameTooShort(_))));
    }

    #[test]
    fn test_invalid_sync_bytes() {
        let mut data = vec![0x00; FRAME_HEADER_SIZE + FRAME_FOOTER_SIZE];
        data[0] = 0xFF; // Wrong sync byte
        data[1] = 0xAA;

        let result = Frame::decode(&data);
        assert!(matches!(result, Err(FrameError::InvalidSyncBytes(_, _))));
    }

    #[test]
    fn test_read_frame_from_buffer_incomplete() {
        // Buffer with partial header
        let partial_data = vec![0x55, 0xAA, 0x01];
        let (frame, consumed, error) = read_frame_from_buffer(&partial_data);
        assert!(frame.is_none());
        assert_eq!(consumed, 0);
        assert!(error.is_none());
    }

    #[test]
    fn test_read_frame_from_buffer_success() {
        let payload = b"Complete frame";
        let encoded = Frame::encode(5, FRAME_TYPE_RESPONSE, 0x1234, payload).unwrap();

        let (frame, consumed, error) = read_frame_from_buffer(&encoded);
        assert!(error.is_none());
        assert_eq!(consumed, encoded.len());

        let frame = frame.unwrap();
        assert_eq!(frame.sequence, 5);
        assert_eq!(frame.frame_type, FRAME_TYPE_RESPONSE);
        assert_eq!(frame.flags, 0x1234);
        assert_eq!(frame.payload, payload);
    }

    #[test]
    fn test_read_frame_with_garbage_prefix() {
        let payload = b"Frame after garbage";
        let mut encoded = Frame::encode(10, FRAME_TYPE_REQUEST, 0, payload).unwrap();

        // Prepend garbage data
        let mut buffer = vec![0xFF, 0x00, 0x11, 0x22];
        buffer.append(&mut encoded);

        let (frame, consumed, error) = read_frame_from_buffer(&buffer);
        assert!(error.is_none());
        assert_eq!(consumed, buffer.len());

        let frame = frame.unwrap();
        assert_eq!(frame.sequence, 10);
        assert_eq!(frame.payload, payload);
    }

    #[test]
    fn test_max_payload_size() {
        let max_payload = vec![0xAB; FRAME_MAX_PAYLOAD];
        let encoded = Frame::encode(255, FRAME_TYPE_REQUEST, 0xFFFF, &max_payload).unwrap();
        let decoded = Frame::decode(&encoded).unwrap();

        assert_eq!(decoded.sequence, 255);
        assert_eq!(decoded.payload.len(), FRAME_MAX_PAYLOAD);
        assert_eq!(decoded.payload, max_payload);
    }

    #[test]
    fn test_empty_payload() {
        let empty_payload = &[];
        let encoded = Frame::encode(0, FRAME_TYPE_RESPONSE, 0, empty_payload).unwrap();
        let decoded = Frame::decode(&encoded).unwrap();

        assert_eq!(decoded.payload.len(), 0);
        assert!(decoded.payload.is_empty());
    }

    #[test]
    fn test_sequence_wraparound() {
        // Test sequence numbers at boundary conditions
        for seq in [0, 127, 128, 254, 255] {
            let payload = b"Sequence test";
            let encoded = Frame::encode(seq, FRAME_TYPE_REQUEST, 0, payload).unwrap();
            let decoded = Frame::decode(&encoded).unwrap();
            assert_eq!(decoded.sequence, seq);
        }
    }
}
