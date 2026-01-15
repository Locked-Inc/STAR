# Phase 2: Gateway gRPC Services Implementation Tracker

**Branch:** `feature/gateway-services-phase2`
**Status:** 🚧 In Progress
**Issue:** #181
**Started:** 2026-01-14

---

## Quick Reference Commands

### Build & Test
```bash
# Navigate to gateway directory
cd star-gateway

# Build the gateway binary
go build ./cmd/star-gateway

# Run all tests
go test ./...

# Run tests with verbose output
go test -v ./internal/service/...

# Run specific service tests
go test -v ./internal/service -run TestTelemetryService

# Run with coverage
go test -cover ./internal/service/...

# Generate coverage report
go test -coverprofile=coverage.out ./internal/service/...
go tool cover -html=coverage.out -o coverage.html

# Check coverage percentage
go tool cover -func=coverage.out | grep total

# Run tests with race detection
go test -race ./...
```

### Code Quality
```bash
# Format all Go files
gofmt -w ./internal/service/

# Check what gofmt would change (dry-run)
gofmt -d ./internal/service/

# Run golangci-lint (if installed)
golangci-lint run ./internal/service/

# Vet for suspicious constructs
go vet ./...
```

### Git Commands
```bash
# Check current branch
git branch --show-current

# View status
git status

# Stage changes
git add star-gateway/internal/service/telemetry.go
git add star-gateway/internal/service/telemetry_test.go

# Commit with conventional commit message
git commit -m "feat(gateway): implement TelemetryService with streaming support

Implement TelemetryService with 3 methods:
- GetTelemetry - snapshot of sensor data
- StreamTelemetry - server streaming at 1-100Hz
- GetSystemStatus - firmware version and health

Follows MotorControlService patterns:
- HARQ + Dispatcher integration
- WireMessage wrapping
- RWMutex for thread-safe caching
- Defer unsubscribe to prevent leaks

Unit tests added with 80%+ coverage."

# Push to remote
git push origin feature/gateway-services-phase2

# View commit history
git log --oneline -10

# Show diff
git diff
git diff --cached  # staged changes
```

### Manual Testing
```bash
# Start the gateway
./star-gateway

# In another terminal, test with grpcurl
grpcurl -plaintext -d '{"header":{"request_id":"test-1"}}' \
    localhost:50051 star.v1.TelemetryService/GetSystemStatus

# Test streaming (will stream indefinitely, Ctrl+C to stop)
grpcurl -plaintext -d '{"header":{"request_id":"test-2"},"rate_hz":10}' \
    localhost:50051 star.v1.TelemetryService/StreamTelemetry
```

### Dependency Check
```bash
# Verify dependencies are up to date
go mod tidy

# Download dependencies
go mod download

# Verify dependencies
go mod verify
```

---

## Implementation Progress

### Service 1: TelemetryService ⭐ (2-3 days)
**Priority:** HIGH - Foundation for other services
**Started:** 2026-01-14
**Completed:** 2026-01-14 ✅

**Status:** Complete - All core functionality implemented with graceful shutdown. GetSystemStatus uses mock data pending firmware integration.

- [x] **GetTelemetry** - Unary RPC for snapshot
  - [x] Implement method with validation
  - [x] Add telemetryHolder for thread-safe cached telemetry
  - [x] Background goroutine for updates (with graceful shutdown)
  - [x] Tests: Success, NilRequest, NoDataAvailable

- [x] **StreamTelemetry** - Server streaming
  - [x] Validate rate_hz (MinRateHz=1, MaxRateHz=100, DefaultRateHz=10)
  - [x] Subscribe to dispatcher (defer unsubscribe!)
  - [x] Separate receive/send goroutines (receiveStreamTelemetryLoop, streamTelemetryLoop)
  - [x] Tests: ValidRate, InvalidRate, ContextCancellation, MultipleClients, SendError

- [x] **GetSystemStatus** - Unary RPC for health ⚠️ Mock implementation
  - [ ] Create SystemStatusRequest message in wire.proto
  - [ ] Wrap in WireMessage
  - [ ] Send via HARQ
  - [x] Tests: Success, NilRequest
  - [ ] Tests: HarqFailure, Timeout (pending HARQ integration)

- [x] **Register in main.go**
  - [x] Add `NewTelemetryService(ctx, harqHandler, msgDispatcher, logger)`
  - [x] Add `RegisterTelemetryServiceServer(grpcServer, telemetrySvc)`
  - [x] Add graceful shutdown with `telemetrySvc.Shutdown()`

- [x] **Unit Tests**
  - [x] 23 test cases (exceeds 10+ requirement)
  - [x] 87.6% coverage (exceeds 80% requirement)
  - [x] All tests pass

**Files Modified:**
- `internal/service/telemetry.go`
- `internal/service/telemetry_test.go`
- `cmd/star-gateway/main.go`

**Commands to verify:**
```bash
cd star-gateway
go test -v ./internal/service -run Telemetry
go test -coverprofile=coverage.out ./internal/service
go tool cover -func=coverage.out | grep telemetry
go build ./cmd/star-gateway
```

---

### Service 2: ConfigurationService (2-3 days)
**Priority:** MEDIUM - No hardware dependency
**Started:** 2026-01-14
**Completed:** 2026-01-14

- [x] **GetConfiguration** - Read system config
  - [x] Send request via HARQ
  - [x] Parse SystemConfiguration
  - [x] Tests: Success, NilRequest, HarqFailure

- [x] **SetConfiguration** - Apply config with validation
  - [x] Call ValidateConfiguration internally
  - [x] Wrap in WireMessage
  - [x] Support persist_to_nvs flag
  - [x] Tests: Valid, Invalid, ValidationErrors

- [x] **ValidateConfiguration** - Dry-run validation
  - [x] Validate PID gains (kp/ki/kd > 0)
  - [x] Validate output limits (min < max)
  - [x] Validate encoder config (edges_per_rev > 0)
  - [x] Return ConfigValidationResult with errors
  - [x] Tests: ValidConfig, InvalidGains, InvalidLimits

- [x] **ResetToDefaults** - Factory reset
  - [x] Send reset command
  - [x] Tests: Success, HarqFailure

- [x] **SaveConfiguration** - Persist to NVS
  - [x] Send save command
  - [x] Tests: Success, HarqFailure

- [x] **GetMotorPidConfig** - Read PID for motor
  - [x] Validate motor_id
  - [x] Request config via HARQ
  - [x] Tests: Success, InvalidMotorId

- [x] **SetMotorPidConfig** - Update PID runtime
  - [x] Validate motor_id and gains
  - [x] Support persist_to_nvs flag
  - [x] Tests: Success, InvalidGains, RuntimeUpdate

- [x] **Register in main.go**
- [x] **Unit Tests** (12+ cases, 80%+ coverage)

**Files Modified:**
- `internal/service/configuration.go`
- `internal/service/configuration_test.go`
- `cmd/star-gateway/main.go`

**Commands to verify:**
```bash
go test -v ./internal/service -run TestConfigurationService
go test -cover ./internal/service/configuration_test.go
```

---

### Service 3: BatteryManagementService (Deferred)
**Priority:** MEDIUM - Hardware dependent (Issue #158)
**Status:** Deferred to follow-up issue
**Note:** TODO: Follow-up issue #183 for this implementation.

- [ ] **GetBatteryState** - Complete battery snapshot
  - [ ] Wrap in WireMessage.BatteryStateRequest
  - [ ] Parse BatteryState (cells, temps, SOC, status)
  - [ ] Tests: Success, NilRequest, SMBusError

- [ ] **StreamBatteryState** - Continuous monitoring
  - [ ] Validate rate (0.1-10 Hz, default 1)
  - [ ] Subscribe to dispatcher
  - [ ] Tests: ValidRate, InvalidRate, ContextCancellation

- [ ] **GetProtectionThresholds** - Read OV/UV/OC/OT
  - [ ] Request BQ78350 register read
  - [ ] Tests: Success, SMBusError

- [ ] **SetProtectionThresholds** - Configure limits
  - [ ] Validate ranges (prevent dangerous configs)
  - [ ] Send to BQ78350 via SMBus
  - [ ] Log all changes
  - [ ] Tests: Valid, InvalidRanges, UnsealedRequired

- [ ] **EnableCellBalancing** - Start balancing
  - [ ] Parse cell_mask bitmap
  - [ ] Send SMBus command
  - [ ] Tests: Success, InvalidMask

- [ ] **DisableCellBalancing** - Stop balancing
- [ ] **GetBalancingStatus** - Check which cells balancing

- [ ] **ControlFets** - Charge/discharge FET control
  - [ ] Validate command (safety critical!)
  - [ ] Log all FET operations
  - [ ] Tests: Enable, Disable, SafetyCheck

- [ ] **GetDeviceInfo** - Manufacturer, serial, chemistry
- [ ] **ResetDevice** - Soft reset BMS

- [ ] **Register in main.go**
- [ ] **Unit Tests** (15+ cases, 80%+ coverage)

**Files Modified:**
- `internal/service/battery.go`
- `internal/service/battery_test.go`
- `cmd/star-gateway/main.go`

**Commands to verify:**
```bash
go test -v ./internal/service -run TestBatteryManagementService
go test -cover ./internal/service/battery_test.go
```

## Final Verification Checklist

### Build & Test
- [x] All services build without errors: `go build ./cmd/star-gateway`
- [x] All tests pass: `go test ./...`
- [x] Coverage ≥ 80%: `go test -coverprofile=coverage.out ./internal/service/...` (87.6%)
- [ ] No race conditions: `go test -race ./...`

### Code Quality
- [x] Code formatted: `gofmt -d ./internal/service/` (no output)
- [x] Vet passes: `go vet ./...`
- [ ] Lint passes: `golangci-lint run ./internal/service/`

### Service Registration
- [x] 4 services registered in main.go (Gateway, MotorControl, Telemetry, Configuration):
  ```bash
  grep -c "RegisterServiceServer" star-gateway/cmd/star-gateway/main.go
  # Currently: 4 (Target: 6)
  ```
- [x] Services registered: Gateway, MotorControl, Telemetry, Configuration
- [ ] Services pending: BatteryManagement, FirmwareUpdate

### Manual Integration Test
- [x] Gateway starts successfully: `./star-gateway`
- [x] TelemetryService responds:
  ```bash
  grpcurl -plaintext -d '{"header":{"request_id":"test-1"}}' \
      localhost:50051 star.v1.TelemetryService/GetSystemStatus
  ```
- [x] Services listed:
  ```bash
  grpcurl -plaintext localhost:50051 list
  # Shows: star.v1.GatewayService, star.v1.MotorControlService, star.v1.TelemetryService
  ```

### Git & PR
- [ ] All changes committed with conventional commit messages
- [ ] Branch pushed to remote: `git push origin feature/gateway-services-phase2`
- [ ] PR created with template from plan
- [ ] PR title: `feat(gateway): implement remaining gRPC services (Phase 2)`
- [ ] PR links to issue #181
- [ ] PR description includes verification steps

---

## Common Issues & Solutions

### Issue: Tests fail with "panic: runtime error: invalid memory address"
**Solution:** Check for nil pointer dereferences. Ensure all request validations are in place.

### Issue: "dispatcher channel closed" errors
**Solution:** Ensure `defer dispatcher.Unsubscribe()` is called in streaming methods.

### Issue: Race condition detected
**Solution:** Use `sync.RWMutex` for shared state. Read locks for reads, write locks for writes.

### Issue: Coverage below 80%
**Solution:** Add test cases for error paths (nil requests, HARQ failures, validation errors).

### Issue: Build fails with "undefined: starv1.SomeMessage"
**Solution:** Regenerate protobuf code:
```bash
cd ../star-proto
buf generate proto/
```

---

## Testing Patterns Reference

### Mock Setup
```go
mockHARQ := &testutil.MockHARQ{
    SendFunc: func(data []byte) error {
        // Capture payload for verification
        mockHARQ.LastSentPayload = data
        return nil  // or return error for failure tests
    },
}

mockDispatcher := &testutil.MockDispatcher{
    SubscribeFunc: func(msgType dispatcher.MessageType) <-chan *starv1.WireMessage {
        ch := make(chan *starv1.WireMessage, 10)
        // Send test data
        go func() {
            ch <- testWireMessage
        }()
        return ch
    },
}

logger := testutil.NewDiscardLogger()
svc := NewTelemetryService(mockHARQ, mockDispatcher, logger)
```

### Streaming Test Pattern
```go
func TestStreamTelemetry(t *testing.T) {
    ctx, cancel := context.WithCancel(context.Background())
    defer cancel()

    stream := &mockStreamServer{ctx: ctx}
    errChan := make(chan error, 1)

    go func() {
        errChan <- svc.StreamTelemetry(req, stream)
    }()

    time.Sleep(50 * time.Millisecond)  // Let some data flow
    cancel()

    select {
    case err := <-errChan:
        assert.True(t, errors.Is(err, context.Canceled))
    case <-time.After(100 * time.Millisecond):
        t.Fatal("goroutine did not exit")
    }

    assert.Greater(t, len(stream.sentData), 0)
}
```

---

## Key Implementation Patterns

### Constructor
```go
func NewTelemetryService(h harq.HARQ, d dispatcher.Dispatcher, logger *slog.Logger) *TelemetryService {
    return &TelemetryService{
        harqHandler: h,
        dispatcher:  d,
        logger:      logger,
    }
}
```

### WireMessage Wrapping
```go
wrapper := &starv1.WireMessage{
    Payload: &starv1.WireMessage_TelemetryData{
        TelemetryData: telemetry,
    },
}
payload, err := proto.Marshal(wrapper)
```

### Response Header
```go
Header: &starv1.ResponseHeader{
    RequestId:       req.Header.GetRequestId(),
    ServerTimestamp: timestamppb.Now(),
    Status:          starv1.Status_STATUS_OK,
}
```

### Server Streaming Template
```go
func (s *Service) StreamData(req *Request, stream Service_StreamDataServer) error {
    // 1. Validate rate
    rateHz := validateRateHz(req.RateHz)
    ticker := time.NewTicker(time.Second / time.Duration(rateHz))
    defer ticker.Stop()

    // 2. Subscribe to dispatcher
    ctx := stream.Context()
    dataCh := s.dispatcher.Subscribe(dispatcher.MessageTypeData)
    defer s.dispatcher.Unsubscribe(dispatcher.MessageTypeData, dataCh)

    // 3. Thread-safe cache
    var latestData *Data
    var dataMutex sync.RWMutex

    // 4. Background receive goroutine
    go s.receiveDataLoop(ctx, dataCh, &latestData, &dataMutex)

    // 5. Main send loop
    return s.streamDataLoop(ctx, ticker, stream, &latestData, &dataMutex)
}
```

---

## Resources

- **Plan:** `/Users/cesarmagana/.claude/plans/indexed-knitting-sutherland.md`
- **Roadmap:** `star-ros2/ROADMAP.md`
- **Reference:** `star-gateway/internal/service/motor_control.go` (PR #174)
- **Issue:** https://github.com/Locked-Inc/STAR/issues/181
- **PR Template:** See plan file

---

## Notes

- Always wrap messages in WireMessage for protocol multiplexing
- Always defer Unsubscribe to prevent memory leaks
- Use sync.RWMutex for thread-safe caching
- Validate inputs before HARQ send
- Log errors with request_id and full context
- Test concurrent access patterns for streaming
- BatteryService can be mocked until Issue #158 is complete
- FirmwareService requires bootloader (lower priority)

---

**Last Updated:** 2026-01-14
**Next Review:** After each service completion
