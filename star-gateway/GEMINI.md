# GEMINI.md - star-gateway

This file provides guidance to Google Gemini when working with the star-gateway module.

## Module Overview

**star-gateway** is the Go gateway service running on Raspberry Pi 5 that bridges:
- **gRPC clients** (Nav2/ROS2, UI) on the high-level side
- **RX72N motor controller** via SPI on the low-level side

## Architecture

### Protocol Stack

The gateway implements Layers 1-4 of the communication protocol:

| Layer | Name | star-gateway Implementation |
|-------|------|----------------------------|
| 5 | Application | gRPC service handlers (`internal/service/`) |
| 4 | Serialization | protobuf via generated code (`star-proto/gen/go/`) |
| 3 | HARQ + FEC | Chase Combining HARQ (`internal/harq/`) + Convolutional FEC (`internal/fec/`) |
| 2 | Framing | SYNC + Header + CRC-32 (`internal/frame/`) |
| 1 | Transport | SPI at 10 MHz (`internal/transport/`) |

### Directory Structure

```
star-gateway/
├── cmd/
│   └── star-gateway/
│       └── main.go          # Entry point
├── internal/
│   ├── transport/           # Layer 1: SPI transport
│   │   ├── spi.go           # Transport interface and SPITransport
│   │   └── spi_test.go
│   ├── frame/               # Layer 2: Frame protocol
│   │   ├── frame.go         # Frame constants and types
│   │   ├── encoder.go       # Frame encoder
│   │   ├── decoder.go       # Frame decoder
│   │   └── frame_test.go
│   ├── harq/                # Layer 3: HARQ protocol (Chase Combining)
│   │   ├── harq.go          # HARQ interface and ChaseCombining
│   │   └── harq_test.go
│   ├── fec/                 # Forward Error Correction
│   │   ├── fec.go           # FEC interfaces and SoftBit type
│   │   ├── convolutional.go # Rate-1/2, K=7 convolutional encoder
│   │   ├── viterbi.go       # Soft Viterbi decoder
│   │   ├── combiner.go      # Chase Combiner for soft bit combining
│   │   └── fec_test.go
│   ├── arq/                 # Legacy ARQ (deprecated, use harq/)
│   │   ├── arq.go
│   │   └── arq_test.go
│   └── service/             # Layer 5: gRPC services
│       ├── motor_control.go
│       ├── telemetry.go
│       ├── battery.go
│       ├── configuration.go
│       └── firmware.go
├── go.mod
├── go.sum
└── GEMINI.md
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
| Legacy ARQ | Done (deprecated) |
| gRPC services | Placeholder |
| SPI implementation | Not started |
| Integration tests | Not started |

## Related Documentation

- `docs/sections/01_nanopb_protocol.tex` - Protocol specification
- `docs/sections/02_protobuf_schemas.tex` - Protobuf service definitions
- `docs/sections/07_gateway_architecture.tex` - Gateway architecture

## Terminology

Following OSHWA inclusive terminology:
- **Controller/Peripheral** (not master/slave)
- **COPI/CIPO** (not MOSI/MISO)
