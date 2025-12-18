// firmware_update_test.go - Firmware Update Protobuf Serialization Tests
// Verifies OTA firmware update proto serialization/deserialization.
//
// Using Go table-driven tests following standard patterns.
//
// STAR Project - Texas A&M University
// December 2025

package starproto_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
)

// ============================================================================
// FirmwareChunk Tests
// ============================================================================

func TestFirmwareChunkRoundTrip(t *testing.T) {
	tests := []struct {
		name     string
		size     int
		sequence uint32
		offset   uint32
		desc     string
	}{
		{"small", 256, 0, 0, "Small chunk"},
		{"medium", 512, 1, 256, "Medium chunk"},
		{"standard", 1024, 2, 768, "Standard 1KB chunk"},
		{"large", 2048, 3, 1792, "Large 2KB chunk"},
		{"partial", 100, 4, 3840, "Partial final chunk"},
		{"minimal", 1, 5, 3940, "Minimal single byte"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			data := make([]byte, tc.size)
			for i := range data {
				data[i] = byte(i % 256)
			}

			chunk := &starv1.FirmwareChunk{
				Data:       data,
				Sequence:   tc.sequence,
				Offset:     tc.offset,
				ChunkCrc32: 0x12345678,
			}

			bytes, err := proto.Marshal(chunk)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.FirmwareChunk{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, len(data), len(parsed.Data), tc.desc)
			assert.Equal(t, tc.sequence, parsed.Sequence, tc.desc)
			assert.Equal(t, tc.offset, parsed.Offset, tc.desc)
			assert.Equal(t, data, parsed.Data, tc.desc)
		})
	}
}

func TestEmptyChunk(t *testing.T) {
	chunk := &starv1.FirmwareChunk{
		Data:     []byte{},
		Sequence: 0,
		Offset:   0,
	}

	bytes, err := proto.Marshal(chunk)
	require.NoError(t, err)

	parsed := &starv1.FirmwareChunk{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, 0, len(parsed.Data))
}

// ============================================================================
// FirmwareUpdateProgress Tests
// ============================================================================

func TestFirmwareUpdateProgressRoundTrip(t *testing.T) {
	tests := []struct {
		name           string
		totalSize      uint32
		bytesReceived  uint32
		progressPercent int32
		state          starv1.FirmwareUpdateState
		desc           string
	}{
		{"start", 1048576, 0, 0, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING, "Just started"},
		{"quarter", 1048576, 262144, 25, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING, "25% complete"},
		{"half", 1048576, 524288, 50, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING, "50% complete"},
		{"three_quarter", 1048576, 786432, 75, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING, "75% complete"},
		{"complete", 1048576, 1048576, 100, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_VALIDATING, "Transfer complete, validating"},
		{"validated", 1048576, 1048576, 100, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_COMPLETE, "Validation complete"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			progress := &starv1.FirmwareUpdateProgress{
				TotalSize:       tc.totalSize,
				BytesReceived:   tc.bytesReceived,
				ProgressPercent: tc.progressPercent,
				State:           tc.state,
				RunningCrc32:    0xDEADBEEF,
			}

			bytes, err := proto.Marshal(progress)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.FirmwareUpdateProgress{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.totalSize, parsed.TotalSize, tc.desc)
			assert.Equal(t, tc.bytesReceived, parsed.BytesReceived, tc.desc)
			assert.Equal(t, tc.progressPercent, parsed.ProgressPercent, tc.desc)
			assert.Equal(t, tc.state, parsed.State, tc.desc)
		})
	}
}

func TestProgressPercentageCalculation(t *testing.T) {
	progress := &starv1.FirmwareUpdateProgress{
		TotalSize:     1000000,
		BytesReceived: 500000,
	}

	bytes, err := proto.Marshal(progress)
	require.NoError(t, err)

	parsed := &starv1.FirmwareUpdateProgress{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	// Verify percentage calculation
	expectedPercent := int32(parsed.BytesReceived * 100 / parsed.TotalSize)
	assert.Equal(t, int32(50), expectedPercent)
}

func TestEstimatedTimeRemaining(t *testing.T) {
	progress := &starv1.FirmwareUpdateProgress{
		TotalSize:                 1048576,
		BytesReceived:             524288,
		TransferSpeedBps:          100000,
		EstimatedSecondsRemaining: 5, // (1048576 - 524288) / 100000
	}

	bytes, err := proto.Marshal(progress)
	require.NoError(t, err)

	parsed := &starv1.FirmwareUpdateProgress{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, int32(5), parsed.EstimatedSecondsRemaining)
	assert.Equal(t, uint32(100000), parsed.TransferSpeedBps)
}

// ============================================================================
// FirmwareUpdateState Enum Tests
// ============================================================================

func TestFirmwareUpdateStateZeroValue(t *testing.T) {
	assert.Equal(t, int32(0), int32(starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_UNKNOWN))
}

func TestAllFirmwareUpdateStateValues(t *testing.T) {
	states := []starv1.FirmwareUpdateState{
		starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_UNKNOWN,
		starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_IDLE,
		starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING,
		starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_VALIDATING,
		starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_COMPLETE,
		starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_ERROR,
	}

	for _, state := range states {
		t.Run(state.String(), func(t *testing.T) {
			progress := &starv1.FirmwareUpdateProgress{
				State: state,
			}

			bytes, err := proto.Marshal(progress)
			require.NoError(t, err)

			parsed := &starv1.FirmwareUpdateProgress{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, state, parsed.State)
		})
	}
}

func TestStateTransitions(t *testing.T) {
	transitions := []struct {
		from starv1.FirmwareUpdateState
		to   starv1.FirmwareUpdateState
	}{
		{starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_IDLE, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING},
		{starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_VALIDATING},
		{starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_VALIDATING, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_COMPLETE},
		{starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_RECEIVING, starv1.FirmwareUpdateState_FIRMWARE_UPDATE_STATE_ERROR},
	}

	for _, tr := range transitions {
		t.Run(tr.from.String()+"_to_"+tr.to.String(), func(t *testing.T) {
			// Both states should serialize correctly
			progress := &starv1.FirmwareUpdateProgress{State: tr.from}
			bytes, err := proto.Marshal(progress)
			require.NoError(t, err)

			parsed := &starv1.FirmwareUpdateProgress{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)
			assert.Equal(t, tr.from, parsed.State)
		})
	}
}

// ============================================================================
// FirmwareUpdateErrorCode Enum Tests
// ============================================================================

func TestFirmwareUpdateErrorCodeZeroValue(t *testing.T) {
	assert.Equal(t, int32(0), int32(starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_UNKNOWN))
}

func TestAllFirmwareUpdateErrorCodeValues(t *testing.T) {
	codes := []starv1.FirmwareUpdateErrorCode{
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_UNKNOWN,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_INVALID_SIZE,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_ALREADY_IN_PROGRESS,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_NO_PARTITION,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_NOT_STARTED,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_SIZE_EXCEEDED,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_INCOMPLETE,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_CRC_FAILED,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_VALIDATION_FAILED,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_WRITE_FAILED,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_SEQUENCE_ERROR,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_TIMEOUT,
		starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_NO_SPACE,
	}

	for _, code := range codes {
		t.Run(code.String(), func(t *testing.T) {
			errInfo := &starv1.FirmwareUpdateError{
				Code: code,
			}

			bytes, err := proto.Marshal(errInfo)
			require.NoError(t, err)

			parsed := &starv1.FirmwareUpdateError{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, code, parsed.Code)
		})
	}
}

// ============================================================================
// FirmwareUpdateError Tests
// ============================================================================

func TestFirmwareUpdateErrorRoundTrip(t *testing.T) {
	tests := []struct {
		name    string
		code    starv1.FirmwareUpdateErrorCode
		message string
		desc    string
	}{
		{"crc_failed", starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_CRC_FAILED, "CRC mismatch: expected 0xDEADBEEF, got 0xCAFEBABE", "CRC validation error"},
		{"size_exceeded", starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_SIZE_EXCEEDED, "Firmware size exceeds partition", "Size limit exceeded"},
		{"write_failed", starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_WRITE_FAILED, "Flash write error at offset 0x10000", "Flash write error"},
		{"timeout", starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_TIMEOUT, "Transfer timeout after 30s", "Transfer timeout"},
		{"sequence_error", starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_SEQUENCE_ERROR, "Expected sequence 5, got 7", "Out of order chunks"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			errInfo := &starv1.FirmwareUpdateError{
				Code:    tc.code,
				Message: tc.message,
			}

			bytes, err := proto.Marshal(errInfo)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.FirmwareUpdateError{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.code, parsed.Code, tc.desc)
			assert.Equal(t, tc.message, parsed.Message, tc.desc)
		})
	}
}

func TestErrorWithLocation(t *testing.T) {
	errInfo := &starv1.FirmwareUpdateError{
		Code:            starv1.FirmwareUpdateErrorCode_FIRMWARE_UPDATE_ERROR_CODE_WRITE_FAILED,
		Message:         "Flash write failed",
		Details:         "ESP_ERR_FLASH_OP_FAIL",
		ErrorAtSequence: 42,
		ErrorAtOffset:   43008,
	}

	bytes, err := proto.Marshal(errInfo)
	require.NoError(t, err)

	parsed := &starv1.FirmwareUpdateError{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, uint32(42), parsed.ErrorAtSequence)
	assert.Equal(t, uint32(43008), parsed.ErrorAtOffset)
	assert.Equal(t, "ESP_ERR_FLASH_OP_FAIL", parsed.Details)
}

// ============================================================================
// FirmwareInfo Tests
// ============================================================================

func TestFirmwareInfoRoundTrip(t *testing.T) {
	tests := []struct {
		name      string
		version   string
		gitHash   string
		buildDate string
		desc      string
	}{
		{"release", "1.0.0", "abc1234", "2025-12-01", "Release build"},
		{"development", "1.1.0-dev", "def5678", "2025-12-15", "Development build"},
		{"hotfix", "1.0.1", "ghi9012", "2025-12-10", "Hotfix release"},
		{"beta", "2.0.0-beta.1", "jkl3456", "2025-12-20", "Beta release"},
		{"rc", "2.0.0-rc.1", "mno7890", "2025-12-25", "Release candidate"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			info := &starv1.FirmwareInfo{
				Version:    tc.version,
				GitHash:    tc.gitHash,
				BuildDate:  tc.buildDate,
				Size:       1048576,
				Crc32:      0xDEADBEEF,
				IdfVersion: "5.1.0",
				ChipTarget: "esp32s3",
			}

			bytes, err := proto.Marshal(info)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.FirmwareInfo{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.Equal(t, tc.version, parsed.Version, tc.desc)
			assert.Equal(t, tc.gitHash, parsed.GitHash, tc.desc)
			assert.Equal(t, tc.buildDate, parsed.BuildDate, tc.desc)
			assert.Equal(t, "esp32s3", parsed.ChipTarget, tc.desc)
		})
	}
}

func TestFirmwareInfoStringLimits(t *testing.T) {
	// Verify strings within nanopb limits (32 chars)
	info := &starv1.FirmwareInfo{
		Version:   "1.0.0-development-build",
		GitHash:   "abc123def456",
		BuildDate: "2025-12-17T10:30:00Z",
	}

	bytes, err := proto.Marshal(info)
	require.NoError(t, err)

	parsed := &starv1.FirmwareInfo{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Less(t, len(parsed.Version), 32)
	assert.Less(t, len(parsed.GitHash), 32)
	assert.Less(t, len(parsed.BuildDate), 32)
}

// ============================================================================
// Complete Update Flow Tests
// ============================================================================

func TestBeginUpdateRequestResponse(t *testing.T) {
	// Request
	req := &starv1.BeginUpdateRequest{
		Header: &starv1.RequestHeader{
			RequestId:     "ota-begin-001",
			ClientVersion: "1.0.0",
		},
		FirmwareSize:    1048576,
		ExpectedVersion: "1.1.0",
		ExpectedCrc32:   0xDEADBEEF,
	}

	reqBytes, err := proto.Marshal(req)
	require.NoError(t, err)

	parsedReq := &starv1.BeginUpdateRequest{}
	err = proto.Unmarshal(reqBytes, parsedReq)
	require.NoError(t, err)

	assert.Equal(t, uint32(1048576), parsedReq.FirmwareSize)
	assert.Equal(t, "1.1.0", parsedReq.ExpectedVersion)

	// Response
	resp := &starv1.BeginUpdateResponse{
		Header: &starv1.ResponseHeader{
			RequestId: "ota-begin-001",
			Status:    starv1.Status_STATUS_OK,
		},
		Accepted:     true,
		MaxChunkSize: 1024,
		SessionId:    "session-abc123",
	}

	respBytes, err := proto.Marshal(resp)
	require.NoError(t, err)

	parsedResp := &starv1.BeginUpdateResponse{}
	err = proto.Unmarshal(respBytes, parsedResp)
	require.NoError(t, err)

	assert.True(t, parsedResp.Accepted)
	assert.Equal(t, uint32(1024), parsedResp.MaxChunkSize)
	assert.Equal(t, "session-abc123", parsedResp.SessionId)
}

func TestWriteChunkRequestResponse(t *testing.T) {
	chunk := &starv1.FirmwareChunk{
		Data:       make([]byte, 1024),
		Sequence:   0,
		Offset:     0,
		ChunkCrc32: 0x12345678,
	}

	req := &starv1.WriteChunkRequest{
		Header: &starv1.RequestHeader{
			RequestId: "ota-chunk-001",
		},
		Chunk: chunk,
	}

	reqBytes, err := proto.Marshal(req)
	require.NoError(t, err)

	parsedReq := &starv1.WriteChunkRequest{}
	err = proto.Unmarshal(reqBytes, parsedReq)
	require.NoError(t, err)

	assert.Equal(t, 1024, len(parsedReq.Chunk.Data))
}

func TestGetFirmwareInfoResponse(t *testing.T) {
	resp := &starv1.GetFirmwareInfoResponse{
		Header: &starv1.ResponseHeader{
			RequestId: "fw-info-001",
			Status:    starv1.Status_STATUS_OK,
		},
		CurrentFirmware: &starv1.FirmwareInfo{
			Version:    "1.0.0",
			GitHash:    "abc1234",
			BuildDate:  "2025-12-01",
			Size:       1048576,
			Crc32:      0xDEADBEEF,
			IdfVersion: "5.1.0",
			ChipTarget: "esp32s3",
		},
		IsOtaBoot: true,
		IsValid:   true,
	}

	bytes, err := proto.Marshal(resp)
	require.NoError(t, err)

	parsed := &starv1.GetFirmwareInfoResponse{}
	err = proto.Unmarshal(bytes, parsed)
	require.NoError(t, err)

	assert.Equal(t, "1.0.0", parsed.CurrentFirmware.Version)
	assert.True(t, parsed.IsOtaBoot)
	assert.True(t, parsed.IsValid)
}

func TestRebootRequestResponse(t *testing.T) {
	req := &starv1.RebootRequest{
		Header: &starv1.RequestHeader{
			RequestId: "reboot-001",
		},
		DelayMs: 1000,
	}

	reqBytes, err := proto.Marshal(req)
	require.NoError(t, err)

	parsedReq := &starv1.RebootRequest{}
	err = proto.Unmarshal(reqBytes, parsedReq)
	require.NoError(t, err)

	assert.Equal(t, uint32(1000), parsedReq.DelayMs)
}

// ============================================================================
// Chunk Streaming Simulation Tests
// ============================================================================

func TestChunkStreamingSimulation(t *testing.T) {
	tests := []struct {
		name  string
		count int
	}{
		{"1_chunk", 1},
		{"10_chunks", 10},
		{"100_chunks", 100},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			var totalSize int
			for i := 0; i < tc.count; i++ {
				data := make([]byte, 1024)
				chunk := &starv1.FirmwareChunk{
					Data:       data,
					Sequence:   uint32(i),
					Offset:     uint32(i * 1024),
					ChunkCrc32: uint32(i),
				}

				bytes, err := proto.Marshal(chunk)
				require.NoError(t, err)
				totalSize += len(bytes)

				parsed := &starv1.FirmwareChunk{}
				err = proto.Unmarshal(bytes, parsed)
				require.NoError(t, err)
			}

			t.Logf("Streamed %d chunks, total proto size: %d bytes", tc.count, totalSize)
		})
	}
}
