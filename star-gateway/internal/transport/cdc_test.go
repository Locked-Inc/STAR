// Package transport tests for USB CDC transport.
//
// STAR Project - Texas A&M University
// January 2026
package transport

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"testing"
	"time"
)

// Test constants to avoid magic numbers
const (
	testBaudRate              = 9600
	testTimeout               = 50 * time.Millisecond
	testTransferTimeout       = 1 * time.Second
	testVID                   = 0x1234
	testPID                   = 0x5678
	concurrencyTestGoroutines = 10
	testReceiveBufferSize     = 10
	testUSBDevicePath         = "/dev/ttyUSB0"
)

// Test data payloads to avoid magic numbers
var (
	testOperationPayload           = []byte{0x01, 0x02, 0x03}
	testConcurrencySendPayload     = []byte{0x01, 0x02}
	testConcurrencyTransferPayload = []byte{0x03, 0x04}
)

// TestDefaultCDCConfig verifies the default CDC configuration.
func TestDefaultCDCConfig(t *testing.T) {
	cfg := DefaultCDCConfig()

	if cfg.Device != DefaultCDCDevice {
		t.Errorf("expected device %s, got %s", DefaultCDCDevice, cfg.Device)
	}

	if cfg.BaudRate != DefaultBaudRate {
		t.Errorf("expected baud rate %d, got %d", DefaultBaudRate, cfg.BaudRate)
	}

	if cfg.Timeout != DefaultCDCTimeout {
		t.Errorf("expected timeout %v, got %v", DefaultCDCTimeout, cfg.Timeout)
	}

	if cfg.VID != CypressVID {
		t.Errorf("expected VID 0x%04X, got 0x%04X", CypressVID, cfg.VID)
	}

	if cfg.PID != CY7C65213PID {
		t.Errorf("expected PID 0x%04X, got 0x%04X", CY7C65213PID, cfg.PID)
	}
}

// TestNewCDCTransport verifies CDCTransport construction.
func TestNewCDCTransport(t *testing.T) {
	t.Run("WithConfig", func(t *testing.T) {
		cfg := &CDCConfig{
			Device:   "/dev/ttyUSB1",
			BaudRate: testBaudRate,
			Timeout:  testTimeout,
			VID:      testVID,
			PID:      testPID,
		}

		cdc := NewCDCTransport(cfg)

		if cdc == nil {
			t.Fatal("expected non-nil CDCTransport")
		}

		if cdc.config.Device != cfg.Device {
			t.Errorf("expected device %s, got %s", cfg.Device, cdc.config.Device)
		}

		if cdc.config.BaudRate != cfg.BaudRate {
			t.Errorf("expected baud rate %d, got %d", cfg.BaudRate, cdc.config.BaudRate)
		}

		if cdc.IsOpen() {
			t.Error("expected transport to be closed initially")
		}
	})

	t.Run("WithNilConfig", func(t *testing.T) {
		cdc := NewCDCTransport(nil)

		if cdc == nil {
			t.Fatal("expected non-nil CDCTransport")
		}

		// Should use default config
		if cdc.config.Device != DefaultCDCDevice {
			t.Errorf("expected default device %s, got %s", DefaultCDCDevice, cdc.config.Device)
		}

		if cdc.config.BaudRate != DefaultBaudRate {
			t.Errorf("expected default baud rate %d, got %d", DefaultBaudRate, cdc.config.BaudRate)
		}
	})
}

// TestCDCTransportOpenClose tests opening and closing the CDC device.
// Note: This test will skip if /dev/ttyUSB0 is not available.
func TestCDCTransportOpenClose(t *testing.T) {
	cfg := DefaultCDCConfig()
	cdc := NewCDCTransport(cfg)

	// Try to open - may fail if device not present
	err := cdc.Open()
	if err != nil {
		t.Skipf("Skipping test: CDC device not available: %v", err)
	}

	// Verify open state
	if !cdc.IsOpen() {
		t.Error("expected transport to be open after Open()")
	}

	// Try opening again - should be idempotent
	if err := cdc.Open(); err != nil {
		t.Errorf("second Open() should succeed: %v", err)
	}

	// Close the device
	if err := cdc.Close(); err != nil {
		t.Errorf("Close() failed: %v", err)
	}

	// Verify closed state
	if cdc.IsOpen() {
		t.Error("expected transport to be closed after Close()")
	}

	// Try closing again - should be idempotent
	if err := cdc.Close(); err != nil {
		t.Errorf("second Close() should succeed: %v", err)
	}
}

// TestCDCTransportOperationsWhenClosed verifies operations fail when device is closed.
func TestCDCTransportOperationsWhenClosed(t *testing.T) {
	cdc := NewCDCTransport(nil)
	ctx := context.Background()

	tests := []struct {
		name string
		op   func() error
	}{
		{
			name: "Send",
			op: func() error {
				_, err := cdc.Send(testOperationPayload)
				return err
			},
		},
		{
			name: "Receive",
			op: func() error {
				_, err := cdc.Receive(testReceiveBufferSize)
				return err
			},
		},
		{
			name: "Transfer",
			op: func() error {
				_, err := cdc.Transfer(ctx, testOperationPayload)
				return err
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := tt.op(); err != ErrDeviceNotOpen {
				t.Errorf("expected ErrDeviceNotOpen, got %v", err)
			}
		})
	}
}

// TestCDCTransportContextCancellation verifies context handling.
func TestCDCTransportContextCancellation(t *testing.T) {
	// Create a transport (doesn't need to be open for context checks)
	cdc := NewCDCTransport(nil)

	t.Run("CancelledContext", func(t *testing.T) {
		ctx, cancel := context.WithCancel(context.Background())
		cancel() // Cancel immediately

		_, err := cdc.Transfer(ctx, []byte{0x01, 0x02})
		if err != context.Canceled {
			t.Errorf("expected context.Canceled, got %v", err)
		}
	})

	t.Run("DeadlineExceeded", func(t *testing.T) {
		ctx, cancel := context.WithDeadline(context.Background(), time.Now().Add(-1*time.Second))
		defer cancel()

		_, err := cdc.Transfer(ctx, []byte{0x01, 0x02})
		if err != context.DeadlineExceeded {
			t.Errorf("expected context.DeadlineExceeded, got %v", err)
		}
	})
}

// TestCDCTransportConfig verifies Config() accessor.
func TestCDCTransportConfig(t *testing.T) {
	cfg := &CDCConfig{
		Device:   testUSBDevicePath,
		BaudRate: testBaudRate,
		Timeout:  testTimeout,
		VID:      testVID,
		PID:      testPID,
	}

	cdc := NewCDCTransport(cfg)
	gotCfg := cdc.Config()

	if gotCfg.Device != cfg.Device {
		t.Errorf("expected device %s, got %s", cfg.Device, gotCfg.Device)
	}

	if gotCfg.BaudRate != cfg.BaudRate {
		t.Errorf("expected baud rate %d, got %d", cfg.BaudRate, gotCfg.BaudRate)
	}

	if gotCfg.VID != cfg.VID {
		t.Errorf("expected VID 0x%04X, got 0x%04X", cfg.VID, gotCfg.VID)
	}

	if gotCfg.PID != cfg.PID {
		t.Errorf("expected PID 0x%04X, got 0x%04X", cfg.PID, gotCfg.PID)
	}
}

// TestCDCTransportAutoDetect tests auto-detection behavior.
// Note: This test will skip if no CDC devices are available.
func TestCDCTransportAutoDetect(t *testing.T) {
	cfg := &CDCConfig{
		Device:   "", // Empty device triggers auto-detection
		BaudRate: DefaultBaudRate,
		Timeout:  DefaultCDCTimeout,
		VID:      CypressVID,
		PID:      CY7C65213PID,
	}

	cdc := NewCDCTransport(cfg)
	err := cdc.Open()

	if err != nil {
		// Auto-detection may fail if no matching device is found
		if errors.Is(err, ErrDeviceNotFound) {
			t.Skipf("Skipping test: No CDC device found with VID=%04X PID=%04X", cfg.VID, cfg.PID)
		}
		t.Skipf("Skipping test: Auto-detection failed: %v", err)
	}

	defer cdc.Close()

	if !cdc.IsOpen() {
		t.Error("expected transport to be open after successful auto-detection")
	}
}

// TestCDCTransportSendReceive tests basic send/receive operations.
// Note: This test requires a loopback device or real hardware.
// It will skip if the device is not available.
func TestCDCTransportSendReceive(t *testing.T) {
	cfg := DefaultCDCConfig()
	cdc := NewCDCTransport(cfg)

	if err := cdc.Open(); err != nil {
		t.Skipf("Skipping test: CDC device not available: %v", err)
	}
	defer cdc.Close()

	// Test data
	testData := []byte{0x55, 0xAA, 0x01, 0x02, 0x03, 0x04}

	// Send data
	n, err := cdc.Send(testData)
	if err != nil {
		t.Fatalf("Send() failed: %v", err)
	}
	if n != len(testData) {
		t.Errorf("expected to send %d bytes, sent %d", len(testData), n)
	}

	// Receive data (requires loopback or real device responding)
	rxData, err := cdc.Receive(len(testData))
	if err != nil {
		t.Fatalf("Receive() failed: %v", err)
	}

	if len(rxData) != len(testData) {
		t.Errorf("expected to receive %d bytes, got %d", len(testData), len(rxData))
	}

	// In a loopback setup, received data should match sent data
	for i := range testData {
		if i < len(rxData) && rxData[i] != testData[i] {
			t.Errorf("byte %d: expected 0x%02X, got 0x%02X", i, testData[i], rxData[i])
		}
	}
}

// TestCDCTransportTransfer tests full transfer operation.
// Note: This test requires a loopback device or real hardware.
func TestCDCTransportTransfer(t *testing.T) {
	cfg := DefaultCDCConfig()
	cdc := NewCDCTransport(cfg)

	if err := cdc.Open(); err != nil {
		t.Skipf("Skipping test: CDC device not available: %v", err)
	}
	defer cdc.Close()

	ctx, cancel := context.WithTimeout(context.Background(), testTransferTimeout)
	defer cancel()

	testData := []byte{0x55, 0xAA, 0x01, 0x02, 0x03, 0x04}

	rxData, err := cdc.Transfer(ctx, testData)
	if err != nil {
		t.Fatalf("Transfer() failed: %v", err)
	}

	if len(rxData) != len(testData) {
		t.Errorf("expected to receive %d bytes, got %d", len(testData), len(rxData))
	}
}

// TestCDCTransportConcurrentAccess verifies thread safety for read operations.
func TestCDCTransportConcurrentAccess(t *testing.T) {
	cdc := NewCDCTransport(nil)
	var wg sync.WaitGroup

	// Concurrent IsOpen() calls should be safe
	for i := 0; i < concurrencyTestGoroutines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_ = cdc.IsOpen()
		}()
	}
	wg.Wait()

	// Concurrent Config() calls should be safe
	for i := 0; i < concurrencyTestGoroutines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_ = cdc.Config()
		}()
	}
	wg.Wait()
}

// TestCDCTransportConcurrentMixedOperations verifies thread safety for mixed read/write operations.
// Note: This test uses a closed device to avoid hardware dependencies.
func TestCDCTransportConcurrentMixedOperations(t *testing.T) {
	cdc := NewCDCTransport(nil)
	ctx := context.Background()
	var wg sync.WaitGroup

	// Buffered error channel to collect errors from goroutines
	errCh := make(chan error, concurrencyTestGoroutines*2)

	// Mix of read and write operations on closed device
	// This should be safe and return ErrDeviceNotOpen
	for i := 0; i < concurrencyTestGoroutines; i++ {
		wg.Add(3)

		// Read operation
		go func() {
			defer wg.Done()
			_ = cdc.IsOpen()
			_ = cdc.Config()
		}()

		// Write operations (Send) - should return ErrDeviceNotOpen
		go func() {
			defer wg.Done()
			_, err := cdc.Send(testConcurrencySendPayload)
			errCh <- err
		}()

		// Transfer operation - should return ErrDeviceNotOpen
		go func() {
			defer wg.Done()
			_, err := cdc.Transfer(ctx, testConcurrencyTransferPayload)
			errCh <- err
		}()
	}

	wg.Wait()
	close(errCh)

	// Assert all errors after goroutines complete
	for err := range errCh {
		if err != ErrDeviceNotOpen {
			t.Errorf("expected ErrDeviceNotOpen, got %v", err)
		}
	}
}

// Mock-based tests - these always run in CI/CD without hardware

// TestCDCTransportMockPartialWrite verifies handling of partial Write() operations.
func TestCDCTransportMockPartialWrite(t *testing.T) {
	tests := []struct {
		name           string
		data           []byte
		maxPerWrite    int
		expectedCalls  int
		expectedResult int
	}{
		{
			name:           "SingleByte",
			data:           []byte{0x01, 0x02, 0x03, 0x04},
			maxPerWrite:    1,
			expectedCalls:  4,
			expectedResult: 4,
		},
		{
			name:           "TwoBytes",
			data:           []byte{0x01, 0x02, 0x03, 0x04, 0x05},
			maxPerWrite:    2,
			expectedCalls:  3, // 2 + 2 + 1
			expectedResult: 5,
		},
		{
			name:           "ThreeBytes",
			data:           []byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
			maxPerWrite:    3,
			expectedCalls:  3, // 3 + 3 + 1
			expectedResult: 7,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			mock := newMockSerialPort()
			mock.SetPartialWriteMode(tt.maxPerWrite)

			cdc := newCDCTransportWithMock(nil, mock)

			n, err := cdc.Send(tt.data)
			if err != nil {
				t.Fatalf("Send() failed: %v", err)
			}

			if n != tt.expectedResult {
				t.Errorf("expected to send %d bytes, sent %d", tt.expectedResult, n)
			}

			written := mock.GetWrittenData()
			if len(written) != len(tt.data) {
				t.Errorf("expected %d bytes written to buffer, got %d", len(tt.data), len(written))
			}

			for i := range tt.data {
				if written[i] != tt.data[i] {
					t.Errorf("byte %d: expected 0x%02X, got 0x%02X", i, tt.data[i], written[i])
				}
			}

			if mock.GetWriteCallCount() != tt.expectedCalls {
				t.Errorf("expected %d Write() calls, got %d", tt.expectedCalls, mock.GetWriteCallCount())
			}
		})
	}
}

// TestCDCTransportMockPartialRead verifies handling of partial Read() operations.
func TestCDCTransportMockPartialRead(t *testing.T) {
	tests := []struct {
		name          string
		queuedData    []byte
		maxPerRead    int
		requestBytes  int
		expectedCalls int
	}{
		{
			name:          "SingleByteReads",
			queuedData:    []byte{0x01, 0x02, 0x03, 0x04},
			maxPerRead:    1,
			requestBytes:  4,
			expectedCalls: 4,
		},
		{
			name:          "TwoByteReads",
			queuedData:    []byte{0x01, 0x02, 0x03, 0x04, 0x05},
			maxPerRead:    2,
			requestBytes:  5,
			expectedCalls: 3, // 2 + 2 + 1
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			mock := newMockSerialPort()
			mock.SetPartialReadMode(tt.maxPerRead)
			mock.QueueReadData(tt.queuedData)

			cdc := newCDCTransportWithMock(nil, mock)

			// Use Transfer to test the read loop
			ctx := context.Background()
			txData := make([]byte, len(tt.queuedData))

			rxData, err := cdc.Transfer(ctx, txData)
			if err != nil {
				t.Fatalf("Transfer() failed: %v", err)
			}

			if len(rxData) != len(tt.queuedData) {
				t.Errorf("expected %d bytes, got %d", len(tt.queuedData), len(rxData))
			}

			for i := range tt.queuedData {
				if rxData[i] != tt.queuedData[i] {
					t.Errorf("byte %d: expected 0x%02X, got 0x%02X", i, tt.queuedData[i], rxData[i])
				}
			}

			// Note: Read call count includes the write loop's implicit reads
			// We just verify the data came through correctly
		})
	}
}

// TestCDCTransportMockWriteStall verifies detection of write stalls.
func TestCDCTransportMockWriteStall(t *testing.T) {
	mock := newMockSerialPort()
	mock.SetWriteStallBehavior(true) // Write() returns n=0

	cdc := newCDCTransportWithMock(nil, mock)

	testData := []byte{0x01, 0x02, 0x03, 0x04}

	tests := []struct {
		name           string
		operation      func() (interface{}, error)
		expectedN      int
		expectedRxData interface{}
		expectedError  string
	}{
		{
			name: "Send",
			operation: func() (interface{}, error) {
				n, err := cdc.Send(testData)
				return n, err
			},
			expectedN:     0,
			expectedError: "CDC send stalled after 0/4 bytes (no progress)",
		},
		{
			name: "Transfer",
			operation: func() (interface{}, error) {
				ctx := context.Background()
				rxData, err := cdc.Transfer(ctx, testData)
				return rxData, err
			},
			expectedRxData: []byte(nil),
			expectedError:  "CDC transfer write stalled after 0/4 bytes (no progress)",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := tt.operation()

			if err == nil {
				t.Fatal("expected error for write stall, got nil")
			}

			if err.Error() != tt.expectedError {
				t.Errorf("expected error %q, got %q", tt.expectedError, err.Error())
			}

			// Check bytes sent for Send operation
			if tt.name == "Send" {
				if n, ok := result.(int); ok && n != tt.expectedN {
					t.Errorf("expected %d bytes sent, got %d", tt.expectedN, n)
				}
			}

			// Check rxData for Transfer operation
			if tt.name == "Transfer" {
				rxData, ok := result.([]byte)
				if ok && len(rxData) != 0 {
					t.Errorf("expected empty rxData, got %v", result)
				}
			}
		})
	}
}

// TestCDCTransportMockReadTimeout verifies read timeout detection.
func TestCDCTransportMockReadTimeout(t *testing.T) {
	mock := newMockSerialPort()
	mock.SetReadTimeoutBehavior(true) // Read() returns n=0

	cdc := newCDCTransportWithMock(nil, mock)

	t.Run("Receive", func(t *testing.T) {
		rxData, err := cdc.Receive(10)
		if !errors.Is(err, ErrReadTimeout) {
			t.Errorf("expected ErrReadTimeout, got %v", err)
		}

		if rxData != nil {
			t.Errorf("expected nil rxData, got %v", rxData)
		}
	})

	t.Run("Transfer", func(t *testing.T) {
		ctx := context.Background()
		testData := []byte{0x01, 0x02, 0x03, 0x04}

		// Write succeeds, but read times out
		mock.SetWriteStallBehavior(false)

		rxData, err := cdc.Transfer(ctx, testData)
		if !errors.Is(err, ErrReadTimeout) {
			t.Errorf("expected ErrReadTimeout, got %v", err)
		}

		if rxData != nil {
			t.Errorf("expected nil rxData, got %v", rxData)
		}

		// Verify the error message includes progress
		expectedMsg := "CDC transfer read timeout after 0/4 bytes"
		if !contains(err.Error(), expectedMsg) {
			t.Errorf("error message should contain %q, got %q", expectedMsg, err.Error())
		}
	})
}

// TestCDCTransportMockDeviceDisconnect simulates device disconnect errors.
func TestCDCTransportMockDeviceDisconnect(t *testing.T) {
	disconnectErr := errors.New("device disconnected")

	t.Run("WriteFails", func(t *testing.T) {
		mock := newMockSerialPort()
		mock.SetNextError(disconnectErr)

		cdc := newCDCTransportWithMock(nil, mock)

		n, err := cdc.Send([]byte{0x01, 0x02})
		if err == nil {
			t.Fatal("expected error, got nil")
		}

		if !errors.Is(err, disconnectErr) {
			t.Errorf("expected disconnect error wrapped, got %v", err)
		}

		if n != 0 {
			t.Errorf("expected 0 bytes sent, got %d", n)
		}
	})

	t.Run("ReadFails", func(t *testing.T) {
		mock := newMockSerialPort()
		cdc := newCDCTransportWithMock(nil, mock)

		ctx := context.Background()
		testData := []byte{0x01, 0x02}

		// Set read-specific error - write will succeed, read will fail
		mock.SetNextReadError(disconnectErr)

		rxData, err := cdc.Transfer(ctx, testData)
		if err == nil {
			t.Fatal("expected error, got nil")
		}

		// Error should be wrapped
		expectedMsg := "CDC transfer read failed after 0/2 bytes"
		if !contains(err.Error(), expectedMsg) {
			t.Errorf("error should contain %q, got %q", expectedMsg, err.Error())
		}

		if rxData != nil {
			t.Errorf("expected nil rxData, got %v", rxData)
		}
	})
}

// TestCDCTransportMockConcurrentAccess verifies thread-safe mock usage.
func TestCDCTransportMockConcurrentAccess(t *testing.T) {
	mock := newMockSerialPort()
	cdc := newCDCTransportWithMock(nil, mock)

	var wg sync.WaitGroup
	errCh := make(chan error, concurrencyTestGoroutines*2)

	// Queue enough data for all concurrent reads
	for i := 0; i < concurrencyTestGoroutines; i++ {
		mock.QueueReadData([]byte{0x01, 0x02, 0x03, 0x04})
	}

	// Concurrent Transfer operations
	for i := 0; i < concurrencyTestGoroutines; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()

			ctx := context.Background()
			testData := []byte{byte(id), 0x02, 0x03, 0x04}

			_, err := cdc.Transfer(ctx, testData)
			if err != nil {
				errCh <- fmt.Errorf("goroutine %d: %w", id, err)
			}
		}(i)
	}

	wg.Wait()
	close(errCh)

	// Check for any errors
	for err := range errCh {
		t.Errorf("concurrent access error: %v", err)
	}
}

// TestCDCTransportMockLoopback tests send-receive loopback pattern.
func TestCDCTransportMockLoopback(t *testing.T) {
	mock := newMockSerialPort()
	cdc := newCDCTransportWithMock(nil, mock)

	testData := []byte{0x55, 0xAA, 0x01, 0x02, 0x03, 0x04}

	// Queue the data we expect to receive
	mock.QueueReadData(testData)

	ctx := context.Background()
	rxData, err := cdc.Transfer(ctx, testData)
	if err != nil {
		t.Fatalf("Transfer() failed: %v", err)
	}

	// Verify received data matches
	if len(rxData) != len(testData) {
		t.Fatalf("expected %d bytes, got %d", len(testData), len(rxData))
	}

	for i := range testData {
		if rxData[i] != testData[i] {
			t.Errorf("byte %d: expected 0x%02X, got 0x%02X", i, testData[i], rxData[i])
		}
	}

	// Verify transmitted data
	written := mock.GetWrittenData()
	if len(written) != len(testData) {
		t.Errorf("expected %d bytes written, got %d", len(testData), len(written))
	}

	for i := range testData {
		if written[i] != testData[i] {
			t.Errorf("written byte %d: expected 0x%02X, got 0x%02X", i, testData[i], written[i])
		}
	}
}

// Helper function for substring checking
func contains(s, substr string) bool {
	return len(s) >= len(substr) && (s == substr || len(s) > len(substr) &&
		(s[:len(substr)] == substr || s[len(s)-len(substr):] == substr ||
			containsSubstring(s, substr)))
}

func containsSubstring(s, substr string) bool {
	for i := 0; i <= len(s)-len(substr); i++ {
		if s[i:i+len(substr)] == substr {
			return true
		}
	}
	return false
}

// Test constants for timeout restoration
const (
	testTimeoutShort   = 500 * time.Millisecond
	testTimeoutLong    = 1 * time.Second
	testDeadlineOffset = 2 * time.Second
)

var testPayload = []byte{0x01, 0x02, 0x03, 0x04}

// TestCDCTransportMockTimeoutRestoration verifies timeout is restored after Transfer().
func TestCDCTransportMockTimeoutRestoration(t *testing.T) {
	tests := []struct {
		name            string
		setupContext    func() (context.Context, context.CancelFunc)
		shouldSucceed   bool
		expectedTimeout time.Duration
	}{
		{
			name: "NormalTransfer",
			setupContext: func() (context.Context, context.CancelFunc) {
				return context.WithTimeout(context.Background(), testTimeoutShort)
			},
			shouldSucceed:   true,
			expectedTimeout: DefaultCDCTimeout,
		},
		{
			name: "CustomDeadline",
			setupContext: func() (context.Context, context.CancelFunc) {
				deadline := time.Now().Add(testDeadlineOffset)
				return context.WithDeadline(context.Background(), deadline)
			},
			shouldSucceed:   true,
			expectedTimeout: DefaultCDCTimeout,
		},
		{
			name: "TransferWithError",
			setupContext: func() (context.Context, context.CancelFunc) {
				return context.WithTimeout(context.Background(), testTimeoutLong)
			},
			shouldSucceed:   false, // Will fail due to read timeout
			expectedTimeout: DefaultCDCTimeout,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			mock := newMockSerialPort()
			cdc := newCDCTransportWithMock(nil, mock)

			testData := testPayload

			if tt.shouldSucceed {
				// Queue data for successful transfer
				mock.QueueReadData(testData)
			} else {
				// Don't queue data - will cause timeout
				mock.SetReadTimeoutBehavior(true)
			}

			// Clear timeout tracking
			mock.mu.Lock()
			mock.timeoutCalls = nil
			mock.mu.Unlock()

			ctx, cancel := tt.setupContext()
			defer cancel()

			_, err := cdc.Transfer(ctx, testData)

			if tt.shouldSucceed && err != nil {
				t.Errorf("Transfer() should succeed but got error: %v", err)
			}

			if !tt.shouldSucceed && err == nil {
				t.Error("Transfer() should fail but got no error")
			}

			// Verify timeout was restored to default
			finalTimeout := mock.GetCurrentTimeout()
			if finalTimeout != tt.expectedTimeout {
				t.Errorf("expected timeout restored to %v, got %v", tt.expectedTimeout, finalTimeout)
			}

			// Verify SetReadTimeout was called (context had deadline)
			timeoutCalls := mock.GetTimeoutCalls()
			if len(timeoutCalls) < 1 {
				t.Error("expected at least one SetReadTimeout call")
			}

			// Last call should be the restoration
			lastCall := timeoutCalls[len(timeoutCalls)-1]
			if lastCall != DefaultCDCTimeout {
				t.Errorf("last timeout call should restore default %v, got %v", DefaultCDCTimeout, lastCall)
			}
		})
	}
}

// TestCDCTransportMockTimeoutRestorationOnPanic verifies timeout restoration with defer.
func TestCDCTransportMockTimeoutRestorationOnPanic(t *testing.T) {
	// Note: This test verifies the defer mechanism works correctly.
	// In practice, panics shouldn't occur, but defer ensures cleanup.

	mock := newMockSerialPort()
	cdc := newCDCTransportWithMock(nil, mock)

	testData := []byte{0x01, 0x02, 0x03, 0x04}
	mock.QueueReadData(testData)

	ctx, cancel := context.WithTimeout(context.Background(), 500*time.Millisecond)
	defer cancel()

	// Normal execution - verify defer runs
	_, err := cdc.Transfer(ctx, testData)
	if err != nil {
		t.Fatalf("Transfer() failed: %v", err)
	}

	// Verify timeout was restored
	finalTimeout := mock.GetCurrentTimeout()
	if finalTimeout != DefaultCDCTimeout {
		t.Errorf("defer should restore timeout to %v, got %v", DefaultCDCTimeout, finalTimeout)
	}
}

// TestCDCTransportMockNoDeadlineNoTimeout verifies behavior without context deadline.
func TestCDCTransportMockNoDeadlineNoTimeout(t *testing.T) {
	mock := newMockSerialPort()
	cdc := newCDCTransportWithMock(nil, mock)

	testData := []byte{0x01, 0x02, 0x03, 0x04}
	mock.QueueReadData(testData)

	// Clear timeout tracking
	mock.mu.Lock()
	mock.timeoutCalls = nil
	mock.mu.Unlock()

	// Context without deadline
	ctx := context.Background()

	_, err := cdc.Transfer(ctx, testData)
	if err != nil {
		t.Fatalf("Transfer() failed: %v", err)
	}

	// Verify SetReadTimeout was NOT called (no deadline to set)
	timeoutCalls := mock.GetTimeoutCalls()
	if len(timeoutCalls) != 0 {
		t.Errorf("expected no SetReadTimeout calls without deadline, got %d calls", len(timeoutCalls))
	}

	// Current timeout should still be default (unchanged)
	currentTimeout := mock.GetCurrentTimeout()
	if currentTimeout != DefaultCDCTimeout {
		t.Errorf("timeout should remain at default %v, got %v", DefaultCDCTimeout, currentTimeout)
	}
}
