package frame

import (
	"bytes"
	"encoding/binary"
	"io"
	"testing"
)

// buildTestFrame helper constructs a frame byte slice manually.
func buildTestFrame(seq uint16, flags Flags, payload []byte) []byte {
	// Frame size: SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) + PAYLOAD(N) + CRC(4)
	size := 2 + 2 + 2 + 1 + 1 + len(payload) + 4
	buf := make([]byte, size)
	offset := 0

	// SYNC
	binary.BigEndian.PutUint16(buf[offset:], SyncWord)
	offset += 2

	// SEQ
	binary.BigEndian.PutUint16(buf[offset:], seq)
	offset += 2

	// LEN
	binary.BigEndian.PutUint16(buf[offset:], uint16(len(payload)))
	offset += 2

	// TYPE (Default to Command for testing)
	buf[offset] = byte(FrameTypeCommand)
	offset++

	// FLAGS
	buf[offset] = byte(flags)
	offset++

	// PAYLOAD
	copy(buf[offset:], payload)
	offset += len(payload)

	// CRC
	crc := computeCRC32(buf[:offset])
	binary.BigEndian.PutUint32(buf[offset:], crc)

	return buf
}

func TestStreamDecoder_ValidFrame(t *testing.T) {
	testCases := []struct {
		name    string
		seq     uint16
		flags   Flags
		payload []byte
	}{
		{
			name:    "simple_payload",
			seq:     0x0001,
			flags:   FlagNone,
			payload: []byte("Hello STAR"),
		},
		{
			name:    "empty_payload",
			seq:     0x0002,
			flags:   FlagRequiresAck,
			payload: []byte{},
		},
		{
			name:    "max_payload",
			seq:     0x0003,
			flags:   FlagPriority,
			payload: make([]byte, MaxPayloadSize),
		},
		{
			name:    "binary_payload",
			seq:     0x0004,
			flags:   FlagFECEnabled,
			payload: []byte{0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			frameBytes := buildTestFrame(tc.seq, tc.flags, tc.payload)
			decoder := NewStreamDecoder(bytes.NewReader(frameBytes))
			decoded, err := decoder.Decode()

			if err != nil {
				t.Fatalf("Decode failed: %v", err)
			}

			if decoded.Header.Sequence != tc.seq {
				t.Errorf("Expected sequence 0x%04X, got 0x%04X", tc.seq, decoded.Header.Sequence)
			}
			if decoded.Header.Flags != tc.flags {
				t.Errorf("Expected flags 0x%02X, got 0x%02X", tc.flags, decoded.Header.Flags)
			}
			if !bytes.Equal(decoded.Payload, tc.payload) {
				t.Errorf("Payload mismatch. Expected %d bytes, got %d bytes", len(tc.payload), len(decoded.Payload))
			}

			// Verify metrics
			metrics := decoder.GetMetrics()
			if metrics.TotalFrames != 1 {
				t.Errorf("Expected 1 frame decoded, got %d", metrics.TotalFrames)
			}
			if metrics.CRCErrors != 0 || metrics.SyncErrors != 0 || metrics.LengthErrors != 0 {
				t.Errorf("Expected zero errors, got SyncErrors=%d, CRCErrors=%d, LengthErrors=%d",
					metrics.SyncErrors, metrics.CRCErrors, metrics.LengthErrors)
			}
		})
	}
}

func TestStreamDecoder_GarbagePrefix(t *testing.T) {
	payload := []byte("Resync Me")
	frameBytes := buildTestFrame(0x0002, FlagNone, payload)

	// Prepend garbage
	input1 := append([]byte{0x00, 0x01, 0x02}, frameBytes...)

	decoder := NewStreamDecoder(bytes.NewReader(input1))
	decoded, err := decoder.Decode()

	if err != nil {
		t.Fatalf("Resync failed: %v", err)
	}
	if !bytes.Equal(decoded.Payload, payload) {
		t.Errorf("Payload mismatch after resync")
	}

	metrics := decoder.GetMetrics()
	if metrics.SyncErrors == 0 {
		t.Error("Expected SyncErrors > 0")
	}
}

func TestStreamDecoder_CRCError(t *testing.T) {
	// Build a corrupted frame - corrupt first byte of payload
	payload1 := []byte("Corrupt")
	frame1 := buildTestFrame(0x0003, FlagNone, payload1)
	// Corrupt payload: SYNC(2) + HEADER(6) = offset 8
	const corruptPayloadOffset = SyncSize + HeaderSize
	frame1[corruptPayloadOffset] ^= 0xFF

	// Build a valid frame following it
	payload2 := []byte("Valid")
	frame2 := buildTestFrame(0x0004, FlagNone, payload2)

	// Concatenate corrupted frame + valid frame
	input := append(frame1, frame2...)

	decoder := NewStreamDecoder(bytes.NewReader(input))

	// Decoder should skip corrupted frame and return the valid one
	decoded, err := decoder.Decode()
	if err != nil {
		t.Fatalf("Decode failed to resync after CRC error: %v", err)
	}

	// Verify we got the second (valid) frame
	if decoded.Header.Sequence != 0x0004 {
		t.Errorf("Expected sequence 0x0004, got 0x%04X", decoded.Header.Sequence)
	}
	if !bytes.Equal(decoded.Payload, payload2) {
		t.Errorf("Payload mismatch: expected %q, got %q", payload2, decoded.Payload)
	}

	// Verify CRC error was recorded
	metrics := decoder.GetMetrics()
	if metrics.CRCErrors != 1 {
		t.Errorf("Expected 1 CRC error, got %d", metrics.CRCErrors)
	}
	if metrics.TotalFrames != 1 {
		t.Errorf("Expected 1 successfully decoded frame, got %d", metrics.TotalFrames)
	}
}

func TestStreamDecoder_InvalidLength(t *testing.T) {
	// Build a complete frame, then mutate the LEN field to exceed MaxPayloadSize
	dummyPayload := []byte("Dummy")
	frame1 := buildTestFrame(0x0003, FlagNone, dummyPayload)

	// Mutate LEN field: SYNC(2) + SEQ(2) = offset 4
	const lenOffset = SyncSize + SeqSize
	const invalidLength = MaxPayloadSize + 1
	binary.BigEndian.PutUint16(frame1[lenOffset:], invalidLength)
	// Note: CRC is now invalid, but decoder will reject on length check first

	// Build a valid frame to follow
	payload := []byte("Valid")
	frame2 := buildTestFrame(0x0005, FlagNone, payload)

	// Concatenate: invalid-length frame + valid frame
	input := append(frame1, frame2...)

	decoder := NewStreamDecoder(bytes.NewReader(input))
	decoded, err := decoder.Decode()

	if err != nil {
		t.Fatalf("Failed to resync after invalid length: %v", err)
	}
	if decoded.Header.Sequence != 0x0005 {
		t.Errorf("Expected sequence 0x0005, got 0x%04X", decoded.Header.Sequence)
	}
	if !bytes.Equal(decoded.Payload, payload) {
		t.Errorf("Payload mismatch after resync")
	}

	// Verify length error was recorded
	metrics := decoder.GetMetrics()
	if metrics.LengthErrors == 0 {
		t.Error("Expected LengthErrors > 0")
	}
}

// TestStreamDecoder_EOFDuringPayload verifies graceful EOF handling mid-frame.
func TestStreamDecoder_EOFDuringPayload(t *testing.T) {
	// Build a complete frame
	payload := []byte("TestPayload")
	completeFrame := buildTestFrame(0x0006, FlagNone, payload)

	// Truncate frame mid-payload (cut off last 5 bytes of payload + CRC)
	truncatedFrame := completeFrame[:len(completeFrame)-9]

	decoder := NewStreamDecoder(bytes.NewReader(truncatedFrame))
	_, err := decoder.Decode()

	// Should get EOF or UnexpectedEOF, not panic
	if err == nil {
		t.Fatal("Expected error on truncated frame, got nil")
	}
	if err != io.EOF && err != io.ErrUnexpectedEOF {
		t.Errorf("Expected EOF/UnexpectedEOF, got: %v", err)
	}
}

// TestStreamDecoder_MetricsAccuracy verifies metrics under mixed success/failure.
func TestStreamDecoder_MetricsAccuracy(t *testing.T) {
	// Build test data: garbage + valid + corrupted + valid
	var buf bytes.Buffer

	// 3 bytes garbage
	buf.Write([]byte{0x11, 0x22, 0x33})

	// Valid frame
	frame1 := buildTestFrame(0x0001, FlagNone, []byte("Frame1"))
	buf.Write(frame1)

	// Corrupted frame (corrupt payload)
	frame2 := buildTestFrame(0x0002, FlagNone, []byte("Frame2"))
	frame2[SyncSize+HeaderSize] ^= 0xFF // Corrupt first payload byte
	buf.Write(frame2)

	// Valid frame
	frame3 := buildTestFrame(0x0003, FlagNone, []byte("Frame3"))
	buf.Write(frame3)

	decoder := NewStreamDecoder(bytes.NewReader(buf.Bytes()))

	// Decode valid frame 1
	decoded1, err := decoder.Decode()
	if err != nil {
		t.Fatalf("Frame 1 decode failed: %v", err)
	}
	if decoded1.Header.Sequence != 0x0001 {
		t.Errorf("Frame 1: expected seq 0x0001, got 0x%04X", decoded1.Header.Sequence)
	}

	// Decode should skip corrupted frame 2 and return valid frame 3
	decoded3, err := decoder.Decode()
	if err != nil {
		t.Fatalf("Frame 3 decode failed: %v", err)
	}
	if decoded3.Header.Sequence != 0x0003 {
		t.Errorf("Frame 3: expected seq 0x0003, got 0x%04X", decoded3.Header.Sequence)
	}

	// Verify metrics
	metrics := decoder.GetMetrics()
	if metrics.TotalFrames != 2 {
		t.Errorf("Expected 2 total frames, got %d", metrics.TotalFrames)
	}
	if metrics.SyncErrors != 3 {
		t.Errorf("Expected 3 sync errors (garbage), got %d", metrics.SyncErrors)
	}
	if metrics.CRCErrors != 1 {
		t.Errorf("Expected 1 CRC error (corrupted frame), got %d", metrics.CRCErrors)
	}
}

// TestStreamDecoder_MultipleValidFrames verifies streaming behavior.
func TestStreamDecoder_MultipleValidFrames(t *testing.T) {
	const numFrames = 10
	var buf bytes.Buffer

	// Build multiple frames
	for i := range numFrames {
		payload := []byte{byte(i)}
		frame := buildTestFrame(uint16(i), FlagNone, payload)
		buf.Write(frame)
	}

	decoder := NewStreamDecoder(bytes.NewReader(buf.Bytes()))

	// Decode all frames
	for i := range numFrames {
		decoded, err := decoder.Decode()
		if err != nil {
			t.Fatalf("Frame %d decode failed: %v", i, err)
		}
		if decoded.Header.Sequence != uint16(i) {
			t.Errorf("Frame %d: expected seq %d, got %d", i, i, decoded.Header.Sequence)
		}
		if len(decoded.Payload) != 1 || decoded.Payload[0] != byte(i) {
			t.Errorf("Frame %d: payload mismatch", i)
		}
	}

	// Verify metrics
	metrics := decoder.GetMetrics()
	if metrics.TotalFrames != numFrames {
		t.Errorf("Expected %d total frames, got %d", numFrames, metrics.TotalFrames)
	}
	if metrics.SyncErrors != 0 || metrics.CRCErrors != 0 || metrics.LengthErrors != 0 {
		t.Errorf("Expected zero errors, got SyncErrors=%d, CRCErrors=%d, LengthErrors=%d",
			metrics.SyncErrors, metrics.CRCErrors, metrics.LengthErrors)
	}
}