# ADR-001: Migration from ARQ to HARQ

## Status

Accepted (decision); Implementation: shipped on Go gateway and RX72N firmware.

## Date

- Original ADR: 2025-12-26
- Last implementation update reflected here: 2026-04-25

## Context

The STAR project uses SPI/USB CDC communication between the Raspberry Pi 5
and the RX72N motor controller. The original implementation used Stop-and-Wait
ARQ (Automatic Repeat Request) for reliable frame delivery with the following
characteristics:

- **Protocol:** Stop-and-Wait with 16-bit sequence numbers
- **Error Detection:** CRC-32 checksum
- **Error Handling:** Retransmit on NACK or timeout (max 3 retries)
- **Timeout:** 10 ms per attempt
- **Original location:** `star-gateway/internal/arq/` (deleted in commit
  `f79842718`; replaced by `star-gateway/internal/harq/` and
  `star-gateway/internal/fec/`)

### Limitations of the original ARQ

1. **No Error Correction:** ARQ only detects errors via CRC-32; it cannot correct them.
2. **Independent Retries:** Each retransmission is processed independently;
   information from failed attempts is discarded.
3. **Noise Sensitivity:** In noisy environments, repeated failures waste
   bandwidth without improving signal quality.
4. **Transport Migration:** STAR migrated SPI -> USB CDC, increasing the
   relative weight of soft-error recovery.

## Decision

Implement **Chase Combining HARQ (Type I)** with **Rate-1/2 Convolutional
Codes (K=7)** for both the Go gateway and the RX72N C firmware.

### HARQ Type Selection: Chase Combining (Type I)

Chase Combining retransmits identical frames and combines soft bits from all
attempts before decoding:

```
Transmission 1: Soft bits S1 (CRC fail)
Transmission 2: Soft bits S2 (CRC fail)
Combined:       S_combined = S1 + S2 (element-wise)
Decode:         Viterbi(S_combined) -> success
```

**Why Chase Combining over Incremental Redundancy:**
- Simpler implementation (identical retransmissions)
- Lower memory requirements (no puncturing tables)
- Sufficient for our noise conditions
- Easier Go/C compatibility verification

### FEC Algorithm Selection: Convolutional Code (K=7)

| Criterion | Convolutional (K=7) | LDPC | Reed-Solomon |
|-----------|---------------------|------|--------------|
| Memory (decoder) | 2-4 KB | 10-50+ KB | 1-2 KB |
| Soft-decision support | Excellent | Excellent | Poor |
| Coding gain (Eb/N0) | 4-5 dB | 6-10 dB | 3-4 dB |
| Implementation complexity | Medium | High | Low |

**Why Convolutional Codes:**
1. **Memory Efficient:** K=7 Viterbi decoder needs ~3 KB, fitting in the
   RX72N's 512 KB SRAM.
2. **Soft-Decision Native:** Soft output directly supports Chase Combining.
3. **Proven Embedded Use:** GPS, cellular, satellite systems.
4. **Predictable Latency:** Fixed decode time for the 100 Hz control loop.

**Parameters:**
- Rate: 1/2 (2 output bits per input bit)
- Constraint Length: K=7 (64 states)
- Generator Polynomials: G1 = 171 (octal), G2 = 133 (octal) -- NASA standard.
  See `star-gateway/internal/fec/convolutional.go` constants `G1Octal`, `G2Octal`.

## Consequences

### Positive

- **4-5 dB Coding Gain:** Significantly reduces retry frequency at same SNR.
- **Soft Combining:** Failed attempts contribute to eventual success.
- **Transport agnostic:** Works over SPI or USB CDC.
- **Standard Algorithm:** Well-documented, tested implementations available.

### Negative

- **2x Bandwidth Overhead:** Rate-1/2 code doubles encoded payload size.
- **Memory Increase:** Static buffers added; see budget table below.
- **Latency:** Viterbi decoding adds approximately 0.5 ms per frame.
- **Complexity:** More code to maintain in both Go and C.

### Memory Impact on RX72N

| Component | Original | With HARQ |
|-----------|----------|-----------|
| DMA double-buffer | 8.0 KB | 8.0 KB |
| RX/TX buffers | 2.0 KB | 2.0 KB |
| Protobuf structs | 1.5 KB | 1.5 KB |
| ARQ/HARQ state | 4.0 KB | 4.0 KB |
| Soft buffers (3x) | 0 KB | 48.0 KB |
| Viterbi decoder | 0 KB | 3.0 KB |
| **Total** | **15.5 KB** | **~67 KB** |

This is approximately 13 % of the 512 KB SRAM, well within budget. The
67 KB number is a static design budget; the actual map-file figure is
checked by the firmware-build-verify CI workflow.

## Alternatives Considered

### 1. Reed-Solomon Codes
- **Rejected:** No soft-decision benefit; Chase Combining ineffective.
- Would still detect and correct errors, but no accumulation across retries.

### 2. LDPC Codes
- **Rejected:** Memory requirements (10-50 KB+ for parity matrix) too high.
- Iterative decoding has variable latency unsuitable for real-time control.
- Implementation complexity significantly higher.

### 3. Turbo Codes
- **Rejected:** Patent/licensing concerns (though most have expired).
- Higher complexity than convolutional for marginal gain in our application.

### 4. Keep ARQ, Add Simple FEC
- **Rejected:** Without soft combining, most FEC benefit is lost.
- Hard-decision decoding doesn't accumulate information.

## Implementation Plan

1. **Phase 1:** Documentation (ADR, protocol spec update)
2. **Phase 2:** Go FEC codec (`star-gateway/internal/fec/`)
3. **Phase 3:** Go HARQ layer (`star-gateway/internal/harq/`)
4. **Phase 4:** RX72N FEC codec (`star-rx72n-firmware/libs/rx_fec/`)
5. **Phase 5:** RX72N HARQ layer (`star-rx72n-firmware/libs/rx_harq/`)
6. **Phase 6:** Documentation updates
7. **Phase 7:** Testing and verification

## Implementation Status

### Shipped

| Phase | Component | Status | Location / Notes |
|-------|-----------|--------|------------------|
| 1 | ADR-001 | [PASS] Done | This document |
| 1 | Protocol spec update | [PASS] Done | `docs/sections/01_nanopb_protocol.tex` |
| 2 | Go FEC encoder | [PASS] Done | `star-gateway/internal/fec/convolutional.go` |
| 2 | Go Viterbi decoder | [PASS] Done | `star-gateway/internal/fec/viterbi.go` |
| 2 | Go Chase Combiner | [PASS] Done | `star-gateway/internal/fec/combiner.go` |
| 2 | Go FEC tests | [PASS] Done | unit + fuzz tests in `star-gateway/internal/fec/` |
| 3 | Go HARQ layer | [PASS] Done | `star-gateway/internal/harq/harq.go` |
| 3 | Go HARQ tests | [PASS] Done | `star-gateway/internal/harq/harq_test.go` |
| 4 | RX72N FEC encoder | [PASS] Done | `star-rx72n-firmware/libs/rx_fec/` |
| 4 | RX72N Viterbi decoder | [PASS] Done | `star-rx72n-firmware/libs/rx_fec/` |
| 5 | RX72N HARQ layer | [PASS] Done | `star-rx72n-firmware/libs/rx_harq/` |
| 5 | RX72N soft buffers | [PASS] Done | Static allocation (NASA P10 Rule 3 compliant) |
| 6 | CLAUDE.md updates | [PASS] Done | `star-gateway/CLAUDE.md` |
| 7 | Go <-> C compatibility | [PASS] Done | `test_frame_go_compat`, `test_rx_fec` |
| 7 | Integration tests | [PASS] Done | covered by `test_rx_harq` and gateway HARQ tests |
| 7 | Performance verification | [PASS] Done | Viterbi within budget; verified by host coverage build |

### RX72N implementation notes

The RX72N implementation followed these guidelines:

1. **Static Allocation:** No `malloc()` -- all buffers statically allocated
   per NASA Power of 10 Rule 3.
2. **Memory Layout (representative -- see header for canonical sizing):**
   ```c
   #define HARQ_MAX_PAYLOAD      1024
   #define HARQ_SOFT_BUFFER_SIZE (HARQ_MAX_PAYLOAD * 16)  /* Rate 1/2, 8 bits */
   #define HARQ_MAX_COMBINING    3

   static int8_t  s_soft_buffer[HARQ_MAX_COMBINING][HARQ_SOFT_BUFFER_SIZE];
   static int16_t s_combined_buffer[HARQ_SOFT_BUFFER_SIZE];
   ```
3. **Viterbi state:**
   ```c
   #define VITERBI_STATES     64    /* 2^(K-1) for K=7 */
   #define VITERBI_TRACEBACK  35    /* ~5 * K */

   typedef struct {
     int16_t path_metrics[VITERBI_STATES];
     uint8_t survivors[VITERBI_TRACEBACK][8];  /* Bit-packed */
   } viterbi_state_t;
   ```

4. **Shipped layout:**
   ```
   star-rx72n-firmware/libs/rx_fec/
   +-- inc/rx_fec.h
   +-- src/rx_fec.c

   star-rx72n-firmware/libs/rx_harq/
   +-- inc/rx_harq.h
   +-- src/rx_harq.c
   ```

## References

- [Hybrid ARQ - Wikipedia](https://en.wikipedia.org/wiki/Hybrid_automatic_repeat_request)
- [ARQ vs HARQ - RF Wireless World](https://www.rfwireless-world.com/terminology/arq-vs-harq)
- [Hybrid ARQ - Devopedia](https://devopedia.org/hybrid-arq)
- NASA Convolutional Code Standard: Polynomials 171 (octal), 133 (octal).
