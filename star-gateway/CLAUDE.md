# CLAUDE.md - star-gateway

This file provides guidance to Claude Code when working with the star-gateway module.

## Module Overview

**star-gateway** is the Go gateway service running on Raspberry Pi 5 that bridges:
- **gRPC clients** (Nav2/ROS2, UI) on the high-level side
- **RX72N motor controller** via SPI on the low-level side

## Backward Compatibility Policy

**IMPORTANT:** This gateway service is in-house only with **ZERO backward compatibility requirements**. There will never be public releases.

- **Breaking changes are ENCOURAGED** - Refactor APIs freely to improve code quality
- **No compatibility layers** - Delete old code immediately and update all clients
- **Main branch must work** - The only requirement is that main builds and passes tests
- **Update everything together** - When changing gRPC APIs, update ROS2/UI clients in the same PR

**FORBIDDEN (will be rejected):**
```go
// WRONG - No compatibility aliases
var OldFunctionName = NewFunctionName  // [FAIL] Just update call sites

// WRONG - No deprecated exports
// Deprecated: Use NewType instead
type OldType = NewType  // [FAIL] Delete it
```

**CORRECT:**
```go
// [PASS] Rename types and update all references immediately
type MotorControlRequest struct {
    VelocityMPS float32  // Renamed from 'Speed'
}
```

See the main project CLAUDE.md for complete backward compatibility policy.

## Architecture

### Protocol Stack

The gateway implements Layers 1-4 of the communication protocol:

| Layer | Name | star-gateway Implementation |
|-------|------|----------------------------|
| 5 | Application | gRPC service handlers (`internal/service/`) |
| 4 | Serialization | protobuf via generated code (`star-proto/gen/go/`) |
| 3 | HARQ | Chase Combining (`internal/harq/`) + Convolutional FEC (`internal/fec/`) |
| 2 | Framing | SYNC + Header + CRC-32 (`internal/frame/`) |
| 1 | Transport | SPI at 10 MHz (`internal/transport/`) |

### Directory Structure

```
star-gateway/
+-- cmd/
|   +-- star-gateway/
|       +-- main.go          # Entry point
+-- internal/
|   +-- transport/           # Layer 1: SPI transport
|   |   +-- spi.go           # Transport interface and SPITransport
|   |   +-- spi_test.go
|   +-- frame/               # Layer 2: Frame protocol
|   |   +-- frame.go         # Frame constants and types
|   |   +-- encoder.go       # Frame encoder
|   |   +-- decoder.go       # Frame decoder
|   |   +-- frame_test.go
|   +-- harq/                # Layer 3: HARQ protocol (Chase Combining)
|   |   +-- harq.go          # HARQ interface and ChaseCombining
|   |   +-- harq_test.go
|   +-- fec/                 # Forward Error Correction
|   |   +-- fec.go           # FEC interfaces and SoftBit type
|   |   +-- convolutional.go # Rate-1/2, K=7 convolutional encoder
|   |   +-- viterbi.go       # Soft Viterbi decoder
|   |   +-- combiner.go      # Chase Combiner for soft bit combining
|   |   +-- fec_test.go
|   +-- service/             # Layer 5: gRPC services
|       +-- motor_control.go
|       +-- telemetry.go
|       +-- battery.go
|       +-- configuration.go
|       +-- firmware.go
+-- go.mod
+-- go.sum
+-- CLAUDE.md
```

## Build Commands

```bash
# Build the gateway binary
cd star-gateway
go build ./cmd/star-gateway

# Run all tests
go test ./...

# Run tests with verbose output
go test -v ./...

# Run tests with coverage
go test -cover ./...

# Lint (requires golangci-lint)
golangci-lint run

# Format code
go fmt ./...

# Vet for suspicious constructs
go vet ./...
```

## Dependencies

This module depends on:

- **star-proto/gen/go** - Generated protobuf and gRPC code
- **google.golang.org/grpc** - gRPC framework

The dependency on star-proto is resolved via a `replace` directive in `go.mod`:
```go
replace github.com/Locked-Inc/star-proto/gen/go => ../star-proto/gen/go
```

## Key Constants

### Frame Protocol (from `internal/frame/frame.go`)

| Constant | Value | Description |
|----------|-------|-------------|
| `SyncWord` | `0x55AA` | Frame sync marker |
| `MaxPayloadSize` | 1024 | Max payload bytes |
| `HeaderSize` | 6 | SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) |
| `CRCSize` | 4 | CRC-32 checksum |

### SPI Configuration (from `internal/transport/spi.go`)

| Constant | Value | Description |
|----------|-------|-------------|
| `DefaultDevice` | `/dev/spidev0.0` | SPI device path |
| `DefaultSpeedHz` | 10,000,000 | 10 MHz clock |
| `DefaultMode` | 0 | SPI Mode 0 |

### HARQ Parameters (from `internal/harq/harq.go`)

| Constant | Value | Description |
|----------|-------|-------------|
| `DefaultMaxRetries` | 3 | Max transmission attempts |
| `DefaultTimeout` | 10ms | ACK wait timeout |
| `FECEnabled` | true | FEC encoding/decoding active |

### FEC Parameters (from `internal/fec/`)

| Constant | Value | Description |
|----------|-------|-------------|
| `ConstraintLength` | 7 | K=7 convolutional code |
| `NumStates` | 64 | 2^(K-1) trellis states |
| `G1Octal` | 0171 | Generator polynomial 1 (NASA) |
| `G2Octal` | 0133 | Generator polynomial 2 (NASA) |
| `TailBits` | 6 | Zero bits to flush encoder |
| `Rate` | 0.5 | Code rate (1/2) |

## gRPC Services

The gateway exposes 5 gRPC services:

1. **MotorControlService** - Differential drive control, encoder streaming
2. **TelemetryService** - IMU, GPS, system status
3. **BatteryManagementService** - BQ7850 BMS monitoring
4. **ConfigurationService** - Runtime parameters, NVS persistence
5. **FirmwareUpdateService** - OTA updates, rollback

## Implementation Status

| Component | Status |
|-----------|--------|
| Module structure | Done |
| Frame constants/types | Done |
| Transport interface | Done (placeholder) |
| Frame encoder/decoder | Done |
| FEC encoder/decoder | Done |
| HARQ protocol | Done (Chase Combining) |
| gRPC services | Placeholder |
| SPI implementation | Not started |
| Integration tests | Not started |

## Testing

### Running Tests

```bash
# Run all tests
go test ./...

# Run tests with verbose output
go test -v ./...

# Run tests with coverage
go test -cover ./...

# Run full CI test suite (with race detector and detailed coverage)
go test -v -race -coverprofile=coverage.out -covermode=atomic -coverpkg=./... ./...

# Generate coverage report
go tool cover -func=coverage.out

# Generate HTML coverage report
go tool cover -html=coverage.out -o coverage.html

# Run specific package tests
go test -v ./internal/transport/
go test -v ./internal/frame/
go test -v ./internal/harq/

# Run specific test by name
go test -v -run TestSPITransport ./internal/transport/

# Run CDC mock tests (hardware-independent)
go test -v -run TestCDCTransportMock ./internal/transport
```

### Linting

```bash
# Run golangci-lint (requires golangci-lint installed)
golangci-lint run ./...

# With timeout for large projects
golangci-lint run --timeout=5m ./...

# Check go.mod/go.sum tidy
go mod tidy
git diff --exit-code go.mod go.sum

# Run go vet
go vet ./...

# Check formatting
gofmt -l .

# Fix formatting
gofmt -w .
```

### Security

```bash
# Install govulncheck
go install golang.org/x/vuln/cmd/govulncheck@latest

# Run security vulnerability scan
$(go env GOPATH)/bin/govulncheck ./...

# List all dependencies
go list -m all
```

### Benchmarks

```bash
# Run all benchmarks
go test -bench=. -benchmem ./...

# Run benchmarks without running tests
go test -bench=. -benchmem -run=^$ ./...

# Run specific benchmark
go test -bench=BenchmarkSPITransfer ./internal/transport/
```

### Test Coverage Requirements

- **Minimum coverage:** 75%
- **Current coverage:** 80.8% (as of 2026-01-31)
- **Coverage by package:**
  - `internal/controller`: 100%
  - `internal/fec`: 100%
  - `internal/frame`: 100%
  - `internal/harq`: 87.0%
  - `internal/link`: 83.3%
  - `internal/manager`: 97.4%
  - `internal/service`: 94.4%
  - `internal/transport`: 92.1%

### CI/CD

The GitHub Actions workflow `.github/workflows/gateway.yml` runs:

1. **Build and Test** - Full test suite with race detector
2. **Lint** - golangci-lint, go vet, gofmt
3. **Security Scan** - govulncheck
4. **Cross-Compile** - linux/amd64, linux/arm64
5. **Integration Tests** - E2E tests (requires hardware)
6. **Benchmarks** - Performance regression testing
7. **Summary** - Aggregate results

### Test Artifacts

After running tests, the following artifacts are generated:

- `test-output.log` - Full test output
- `coverage.out` - Raw coverage data
- `coverage-summary.txt` - Function-level coverage
- `coverage.html` - Interactive HTML coverage report
- `vet-output.log` - Go vet results
- `golangci-lint.log` - Linting results
- `govulncheck.log` - Security scan results
- `dependencies.txt` - Full dependency list
- `benchmark.txt` - Benchmark results

### Hardware Testing

Hardware-dependent tests (USB CDC, SPI) skip gracefully when hardware is not available:

```bash
# Test with mock transports (no hardware required)
go test -v ./internal/transport/

# Test with real hardware (requires RPi5 + RX72N)
# - USB CDC device at /dev/ttyACM0
# - SPI device at /dev/spidev0.0
go test -v -tags=hardware ./internal/transport/
```

**Hardware validation checklist:**
- [ ] USB CDC device accessible
- [ ] SPI device accessible
- [ ] RX72N firmware running
- [ ] Transport failover working
- [ ] Hot-plug detection working

See [TRANSPORT_TEST_RESULTS.md](TRANSPORT_TEST_RESULTS.md) for detailed test results.

## Related Documentation

- `TRANSPORT_ARCHITECTURE.md` - Transport layer architecture and smart switching
- `TRANSPORT_TEST_RESULTS.md` - Comprehensive test execution results
- `docs/sections/01_nanopb_protocol.tex` - Protocol specification
- `docs/sections/02_protobuf_schemas.tex` - Protobuf service definitions
- `docs/sections/07_gateway_architecture.tex` - Gateway architecture

## Terminology

Following OSHWA inclusive terminology:
- **Controller/Peripheral** (not master/slave)
- **COPI/CIPO** (not MOSI/MISO)
