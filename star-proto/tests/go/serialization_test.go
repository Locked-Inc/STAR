// serialization_test.go - STAR Protobuf Serialization Tests
// Verifies protobuf round-trip serialization for all message types.
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
// Common Message Tests
// ============================================================================

func TestRequestHeaderRoundTrip(t *testing.T) {
	tests := []struct {
		name          string
		requestID     string
		clientVersion string
	}{
		{"basic", "req-001", "1.0.0"},
		{"uuid", "550e8400-e29b-41d4-a716-446655440000", "2.1.3"},
		{"empty_version", "req-002", ""},
		{"long_id", "very-long-request-identifier-for-testing-purposes", "1.0.0-beta"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			header := &starv1.RequestHeader{
				RequestId:     tc.requestID,
				ClientVersion: tc.clientVersion,
			}

			bytes, err := proto.Marshal(header)
			require.NoError(t, err, "Marshal should succeed")

			parsed := &starv1.RequestHeader{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, "Unmarshal should succeed")

			assert.Equal(t, tc.requestID, parsed.RequestId)
			assert.Equal(t, tc.clientVersion, parsed.ClientVersion)
		})
	}
}

func TestResponseHeaderRoundTrip(t *testing.T) {
	tests := []struct {
		name      string
		requestID string
		status    starv1.Status
	}{
		{"ok", "req-001", starv1.Status_STATUS_OK},
		{"error", "req-002", starv1.Status_STATUS_INTERNAL_ERROR},
		{"invalid", "req-003", starv1.Status_STATUS_INVALID_REQUEST},
		{"timeout", "req-004", starv1.Status_STATUS_TIMEOUT},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			header := &starv1.ResponseHeader{
				RequestId: tc.requestID,
				Status:    tc.status,
			}

			bytes, err := proto.Marshal(header)
			require.NoError(t, err)

			parsed := &starv1.ResponseHeader{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, tc.requestID, parsed.RequestId)
			assert.Equal(t, tc.status, parsed.Status)
		})
	}
}

func TestStatusEnumZeroValue(t *testing.T) {
	// Boston Dynamics style: zero value must be UNKNOWN
	assert.Equal(t, int32(0), int32(starv1.Status_STATUS_UNKNOWN))
}

func TestAllStatusEnumValues(t *testing.T) {
	statuses := []starv1.Status{
		starv1.Status_STATUS_UNKNOWN,
		starv1.Status_STATUS_OK,
		starv1.Status_STATUS_INVALID_REQUEST,
		starv1.Status_STATUS_INTERNAL_ERROR,
		starv1.Status_STATUS_NOT_FOUND,
		starv1.Status_STATUS_TIMEOUT,
		starv1.Status_STATUS_INVALID_STATE,
		starv1.Status_STATUS_ESTOP_ACTIVE,
	}

	for _, status := range statuses {
		t.Run(status.String(), func(t *testing.T) {
			header := &starv1.ResponseHeader{
				RequestId: "test",
				Status:    status,
			}

			bytes, err := proto.Marshal(header)
			require.NoError(t, err)

			parsed := &starv1.ResponseHeader{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, status, parsed.Status)
		})
	}
}

// ============================================================================
// Motor Control Tests
// ============================================================================

func TestVelocityCommandRoundTrip(t *testing.T) {
	tests := []struct {
		name     string
		leftMps  float64
		rightMps float64
		sequence uint32
		desc     string
	}{
		{"forward_full", 2.0, 2.0, 1, "Both wheels forward at max speed"},
		{"reverse_full", -2.0, -2.0, 2, "Both wheels reverse at max speed"},
		{"stop", 0.0, 0.0, 3, "Both wheels stopped"},
		{"turn_left", 0.5, 1.5, 4, "Left turn - right wheel faster"},
		{"turn_right", 1.5, 0.5, 5, "Right turn - left wheel faster"},
		{"spin_left", -1.0, 1.0, 6, "Spin in place counterclockwise"},
		{"spin_right", 1.0, -1.0, 7, "Spin in place clockwise"},
		{"precision_low", 0.001, 0.001, 8, "Very low velocity for precision"},
		{"asymmetric", 1.234, -0.567, 9, "Asymmetric decimal velocities"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			cmd := &starv1.VelocityCommand{
				LeftVelocityMps:  tc.leftMps,
				RightVelocityMps: tc.rightMps,
				Sequence:         tc.sequence,
			}

			bytes, err := proto.Marshal(cmd)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.VelocityCommand{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.InDelta(t, tc.leftMps, parsed.LeftVelocityMps, 0.0001, tc.desc)
			assert.InDelta(t, tc.rightMps, parsed.RightVelocityMps, 0.0001, tc.desc)
			assert.Equal(t, tc.sequence, parsed.Sequence, tc.desc)
		})
	}
}

func TestEncoderDataRoundTrip(t *testing.T) {
	tests := []struct {
		name        string
		motorID     int32
		ticks       int64
		velocityMps float64
	}{
		{"motor0_rest", 0, 0, 0.0},
		{"motor0_forward", 0, 1000, 0.5},
		{"motor0_reverse", 0, -1000, -0.5},
		{"motor1_forward", 1, 5000, 1.2},
		{"motor2_forward", 2, 10000, 2.0},
		{"motor3_forward", 3, -5000, -1.2},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			encoder := &starv1.EncoderData{
				MotorId:     tc.motorID,
				Ticks:       tc.ticks,
				VelocityMps: tc.velocityMps,
			}

			bytes, err := proto.Marshal(encoder)
			require.NoError(t, err)

			parsed := &starv1.EncoderData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, tc.motorID, parsed.MotorId)
			assert.Equal(t, tc.ticks, parsed.Ticks)
			assert.InDelta(t, tc.velocityMps, parsed.VelocityMps, 0.0001)
		})
	}
}

func TestPidConfigRoundTrip(t *testing.T) {
	tests := []struct {
		name string
		kp   float64
		ki   float64
		kd   float64
		desc string
	}{
		{"matlab_tuned", 0.286, 8.01, 0.0, "MATLAB-tuned gains for velocity control"},
		{"conservative", 0.1, 1.0, 0.01, "Conservative gains for stability"},
		{"aggressive", 2.0, 20.0, 0.5, "Aggressive gains for fast response"},
		{"p_only", 1.0, 0.0, 0.0, "Proportional-only control"},
		{"pi_control", 0.5, 5.0, 0.0, "PI control without derivative"},
		{"pd_control", 1.0, 0.0, 0.2, "PD control without integral"},
		{"disabled", 0.0, 0.0, 0.0, "All gains zero - motor disabled"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			pid := &starv1.PidConfig{
				Kp: tc.kp,
				Ki: tc.ki,
				Kd: tc.kd,
			}

			bytes, err := proto.Marshal(pid)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.PidConfig{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.InDelta(t, tc.kp, parsed.Kp, 0.0001, tc.desc)
			assert.InDelta(t, tc.ki, parsed.Ki, 0.0001, tc.desc)
			assert.InDelta(t, tc.kd, parsed.Kd, 0.0001, tc.desc)
		})
	}
}

// ============================================================================
// Telemetry Tests
// ============================================================================

func TestImuDataRoundTrip(t *testing.T) {
	tests := []struct {
		name    string
		accelX  float64
		accelY  float64
		accelZ  float64
		gyroX   float64
		gyroY   float64
		gyroZ   float64
		desc    string
	}{
		{"rest", 0.0, 0.0, 9.81, 0.0, 0.0, 0.0, "At rest, gravity on Z"},
		{"tilted", 4.9, 0.0, 8.5, 0.0, 0.0, 0.0, "Tilted 30 degrees"},
		{"rotating", 0.0, 0.0, 9.81, 0.0, 0.0, 1.57, "Rotating around Z"},
		{"moving", 2.0, 1.0, 9.81, 0.1, 0.05, 0.2, "Moving with rotation"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			imu := &starv1.ImuData{
				AccelXMps2:   tc.accelX,
				AccelYMps2:   tc.accelY,
				AccelZMps2:   tc.accelZ,
				GyroXRadPerS: tc.gyroX,
				GyroYRadPerS: tc.gyroY,
				GyroZRadPerS: tc.gyroZ,
			}

			bytes, err := proto.Marshal(imu)
			require.NoError(t, err, tc.desc)

			parsed := &starv1.ImuData{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err, tc.desc)

			assert.InDelta(t, tc.accelX, parsed.AccelXMps2, 0.0001, tc.desc)
			assert.InDelta(t, tc.accelY, parsed.AccelYMps2, 0.0001, tc.desc)
			assert.InDelta(t, tc.accelZ, parsed.AccelZMps2, 0.0001, tc.desc)
			assert.InDelta(t, tc.gyroX, parsed.GyroXRadPerS, 0.0001, tc.desc)
			assert.InDelta(t, tc.gyroY, parsed.GyroYRadPerS, 0.0001, tc.desc)
			assert.InDelta(t, tc.gyroZ, parsed.GyroZRadPerS, 0.0001, tc.desc)
		})
	}
}

func TestSystemStatusRoundTrip(t *testing.T) {
	tests := []struct {
		name            string
		connStatus      starv1.ConnectionStatus
		mode            starv1.RobotMode
		esp32Connected  bool
		lidarConnected  bool
		rosConnected    bool
		firmwareVersion string
		uptimeS         uint64
	}{
		{"startup", starv1.ConnectionStatus_CONNECTION_STATUS_UNKNOWN, starv1.RobotMode_ROBOT_MODE_IDLE, false, false, false, "", 0},
		{"connected", starv1.ConnectionStatus_CONNECTION_STATUS_CONNECTED, starv1.RobotMode_ROBOT_MODE_MANUAL, true, true, true, "1.0.0", 3600},
		{"autonomous", starv1.ConnectionStatus_CONNECTION_STATUS_CONNECTED, starv1.RobotMode_ROBOT_MODE_AUTONOMOUS, true, true, true, "2.1.0", 86400},
		{"partial", starv1.ConnectionStatus_CONNECTION_STATUS_CONNECTING, starv1.RobotMode_ROBOT_MODE_IDLE, true, false, false, "1.0.0-beta", 100},
		{"disconnected", starv1.ConnectionStatus_CONNECTION_STATUS_DISCONNECTED, starv1.RobotMode_ROBOT_MODE_EMERGENCY_STOP, false, false, false, "1.0.0", 7200},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			status := &starv1.SystemStatus{
				ConnectionStatus: tc.connStatus,
				Mode:             tc.mode,
				Esp32Connected:   tc.esp32Connected,
				LidarConnected:   tc.lidarConnected,
				RosConnected:     tc.rosConnected,
				FirmwareVersion:  tc.firmwareVersion,
				UptimeS:          tc.uptimeS,
			}

			bytes, err := proto.Marshal(status)
			require.NoError(t, err)

			parsed := &starv1.SystemStatus{}
			err = proto.Unmarshal(bytes, parsed)
			require.NoError(t, err)

			assert.Equal(t, tc.connStatus, parsed.ConnectionStatus)
			assert.Equal(t, tc.mode, parsed.Mode)
			assert.Equal(t, tc.esp32Connected, parsed.Esp32Connected)
			assert.Equal(t, tc.lidarConnected, parsed.LidarConnected)
			assert.Equal(t, tc.rosConnected, parsed.RosConnected)
			assert.Equal(t, tc.firmwareVersion, parsed.FirmwareVersion)
			assert.Equal(t, tc.uptimeS, parsed.UptimeS)
		})
	}
}

// ============================================================================
// Request/Response Pattern Tests
// ============================================================================

func TestSetVelocityRequestResponse(t *testing.T) {
	// Request
	req := &starv1.SetVelocityRequest{
		Header: &starv1.RequestHeader{
			RequestId:     "vel-001",
			ClientVersion: "1.0.0",
		},
		Command: &starv1.VelocityCommand{
			LeftVelocityMps:  1.5,
			RightVelocityMps: 1.5,
			Sequence:         42,
		},
	}

	reqBytes, err := proto.Marshal(req)
	require.NoError(t, err)

	parsedReq := &starv1.SetVelocityRequest{}
	err = proto.Unmarshal(reqBytes, parsedReq)
	require.NoError(t, err)

	assert.Equal(t, "vel-001", parsedReq.Header.RequestId)
	assert.InDelta(t, 1.5, parsedReq.Command.LeftVelocityMps, 0.0001)

	// Response
	resp := &starv1.SetVelocityResponse{
		Header: &starv1.ResponseHeader{
			RequestId: "vel-001",
			Status:    starv1.Status_STATUS_OK,
		},
	}

	respBytes, err := proto.Marshal(resp)
	require.NoError(t, err)

	parsedResp := &starv1.SetVelocityResponse{}
	err = proto.Unmarshal(respBytes, parsedResp)
	require.NoError(t, err)

	assert.Equal(t, "vel-001", parsedResp.Header.RequestId)
	assert.Equal(t, starv1.Status_STATUS_OK, parsedResp.Header.Status)
}

// ============================================================================
// Streaming Simulation Tests
// ============================================================================

func TestEncoderStreamSimulation(t *testing.T) {
	tests := []struct {
		name  string
		count int
	}{
		{"1_message", 1},
		{"10_messages", 10},
		{"100_messages", 100},
		{"1000_messages", 1000},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			var totalSize int
			for i := 0; i < tc.count; i++ {
				encoder := &starv1.EncoderData{
					MotorId:     int32(i % 4),
					Ticks:       int64(i * 100),
					VelocityMps: float64(i) * 0.01,
				}

				bytes, err := proto.Marshal(encoder)
				require.NoError(t, err)
				totalSize += len(bytes)

				parsed := &starv1.EncoderData{}
				err = proto.Unmarshal(bytes, parsed)
				require.NoError(t, err)
			}

			t.Logf("Streamed %d messages, total size: %d bytes", tc.count, totalSize)
		})
	}
}
