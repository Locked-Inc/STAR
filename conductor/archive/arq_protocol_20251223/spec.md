# Track Specification: ARQ Protocol Implementation

## Goal
To complete, verify, and document the Stop-and-Wait Automatic Repeat reQuest (ARQ) protocol implementation in the `star-gateway` service. This ensures reliable data transmission over the SPI link between the Raspberry Pi 5 and the RX72N motor controller.

## Context
The STAR platform uses an SPI link for communication. SPI is inherently unreliable (no built-in ack/retry). The ARQ protocol (Layer 3) provides reliability on top of the raw SPI transport. The core logic exists in `star-gateway/internal/arq/arq.go` but requires comprehensive testing and verification against the protocol specification.

## Requirements

### Functional Requirements
1.  **Stop-and-Wait Logic:** The system must transmit one frame at a time and wait for an ACK before sending the next.
2.  **Retransmission:** If an ACK is not received within `DefaultTimeout` (10ms) or a NACK is received, the frame must be retransmitted.
3.  **Retry Limit:** The system must abort and report an error after `DefaultMaxRetries` (3) failed attempts.
4.  **Sequence Handling:** 
    - Transmit sequence numbers must increment on successful ACK.
    - Receive sequence numbers must tracked expected frames.
    - Duplicate frames (previously received sequence) must be ACKed but not processed.
    - Out-of-order frames must be NACKed.
    - Sequence numbers must wrap around correctly at `DefaultSequenceMax`.
5.  **Frame Types:** Correctly handle `Command`, `Response`, `ACK`, and `NACK` frame types.

### Non-Functional Requirements
-   **Test Coverage:** Must achieve 100% test coverage for the `arq` package.
-   **Thread Safety:** All ARQ state mutations must be thread-safe (mutex protected).
-   **Performance:** No memory leaks; efficient state transitions.

## Verification Criteria
-   All unit tests in `arq_test.go` pass.
-   Mock transport tests verify correct interaction with the transport layer.
-   `go test -race` passes with no race conditions.
-   Code coverage report shows >80% coverage (target 100%).
