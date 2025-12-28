# ADR-001: Migration from ARQ to HARQ with FEC

## Status

Accepted

## Date

2025-12-26

## Context

The STAR project uses SPI communication between Raspberry Pi 5 and RX72N motor controller. The current implementation uses Stop-and-Wait ARQ (Automatic Repeat Request) for reliable frame delivery with the following characteristics:

- **Protocol:** Stop-and-Wait with 16-bit sequence numbers
- **Error Detection:** CRC-32 checksum
- **Error Handling:** Retransmit on NACK or timeout (max 3 retries)
- **Timeout:** 10ms per attempt
- **Location:** `star-gateway/internal/arq/`

### Limitations of Current ARQ

1. **No Error Correction:** ARQ only detects errors via CRC-32; it cannot correct them
2. **Independent Retries:** Each retransmission is processed independently; information from failed attempts is discarded
3. **Noise Sensitivity:** In noisy environments, repeated failures waste bandwidth without improving signal quality
4. **Potential USB Migration:** Future move from SPI to USB increases noise concerns

## Decision

Implement **Chase Combining HARQ (Type I)** with **Rate-1/2 Convolutional Codes (K=7)** for both the Go gateway and RX72N C firmware.

### HARQ Type Selection: Chase Combining (Type I)

Chase Combining retransmits identical frames and combines soft bits from all attempts before decoding:

```
Transmission 1: Soft bits S1 (CRC fail)
Transmission 2: Soft bits S2 (CRC fail)
Combined:       S_combined = S1 + S2 (element-wise)
Decode:         Viterbi(S_combined) → success
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
1. **Memory Efficient:** K=7 Viterbi decoder needs ~3KB, fitting RX72N's 512KB SRAM
2. **Soft-Decision Native:** Soft output directly supports Chase Combining
3. **Proven Embedded Use:** GPS, cellular, satellite systems
4. **Predictable Latency:** Fixed decode time for 100Hz control loop

**Parameters:**
- Rate: 1/2 (2 output bits per input bit)
- Constraint Length: K=7 (64 states)
- Generator Polynomials: G1=171₈, G2=133₈ (NASA standard)

## Consequences

### Positive

- **4-5 dB Coding Gain:** Significantly reduces retry frequency at same SNR
- **Soft Combining:** Failed attempts contribute to eventual success
- **USB Ready:** Prepared for potential transport layer change
- **Standard Algorithm:** Well-documented, tested implementations available

### Negative

- **2x Bandwidth Overhead:** Rate-1/2 code doubles encoded payload size
- **Memory Increase:** ~50KB additional SRAM on RX72N (from 15.5KB to ~67KB)
- **Latency:** Viterbi decoding adds ~0.5ms per frame
- **Complexity:** More code to maintain in both Go and C

### Memory Impact on RX72N

| Component | Current | With HARQ |
|-----------|---------|-----------|
| DMA double-buffer | 8.0 KB | 8.0 KB |
| RX/TX buffers | 2.0 KB | 2.0 KB |
| Protobuf structs | 1.5 KB | 1.5 KB |
| ARQ/HARQ state | 4.0 KB | 4.0 KB |
| Soft buffers (3x) | 0 KB | 48.0 KB |
| Viterbi decoder | 0 KB | 3.0 KB |
| **Total** | **15.5 KB** | **66.5 KB** |

This represents 13% of 512KB SRAM, well within acceptable limits.

## Alternatives Considered

### 1. Reed-Solomon Codes
- **Rejected:** No soft-decision benefit; Chase Combining ineffective
- Would still detect and correct errors, but no accumulation across retries

### 2. LDPC Codes
- **Rejected:** Memory requirements (10-50KB+ for parity matrix) too high
- Iterative decoding has variable latency unsuitable for real-time control
- Implementation complexity significantly higher

### 3. Turbo Codes
- **Rejected:** Patent/licensing concerns (though most have expired)
- Higher complexity than convolutional for marginal gain in our application

### 4. Keep ARQ, Add Simple FEC
- **Rejected:** Without soft combining, most FEC benefit is lost
- Hard-decision decoding doesn't accumulate information

## Implementation Plan

1. **Phase 1:** Documentation (ADR, protocol spec update)
2. **Phase 2:** Go FEC codec (`internal/fec/`)
3. **Phase 3:** Go HARQ layer (refactor `arq/` → `harq/`)
4. **Phase 4:** RX72N FEC codec (`lib/rx_fec/`)
5. **Phase 5:** RX72N HARQ layer (`lib/rx_harq/`)
6. **Phase 6:** Documentation updates
7. **Phase 7:** Testing and verification

## Implementation Status

### Completed (2025-12-26)

| Phase | Component | Status | Notes |
|-------|-----------|--------|-------|
| 1 | ADR-001 | ✅ Done | This document |
| 1 | Protocol spec update | ✅ Done | `docs/sections/01_nanopb_protocol.tex` |
| 2 | Go FEC encoder | ✅ Done | `internal/fec/convolutional.go` |
| 2 | Go Viterbi decoder | ✅ Done | `internal/fec/viterbi.go` |
| 2 | Go Chase Combiner | ✅ Done | `internal/fec/combiner.go` |
| 2 | Go FEC tests | ✅ Done | 31 tests passing |
| 3 | Go HARQ layer | ✅ Done | `internal/harq/harq.go` |
| 3 | Go HARQ tests | ✅ Done | 38 tests passing |
| 6 | CLAUDE.md updates | ✅ Done | `star-gateway/CLAUDE.md` |

### Remaining Work

| Phase | Component | Status | Description |
|-------|-----------|--------|-------------|
| 4 | RX72N FEC encoder | ⏳ Pending | C implementation in `lib/rx_fec/` |
| 4 | RX72N Viterbi decoder | ⏳ Pending | Memory-optimized C implementation |
| 5 | RX72N HARQ layer | ⏳ Pending | C implementation in `lib/rx_harq/` |
| 5 | RX72N soft buffers | ⏳ Pending | Static allocation (no malloc) |
| 7 | Go ↔ C compatibility | ⏳ Pending | Shared test vectors for bit-exact verification |
| 7 | Integration tests | ⏳ Pending | End-to-end HARQ handshake testing |
| 7 | Performance tests | ⏳ Pending | Verify Viterbi < 1ms on RX72N |

### RX72N Implementation Notes

The RX72N implementation should follow these guidelines:

1. **Static Allocation:** No `malloc()` - all buffers statically allocated
2. **Memory Layout:**
   ```c
   #define HARQ_MAX_PAYLOAD      1024
   #define HARQ_SOFT_BUFFER_SIZE (HARQ_MAX_PAYLOAD * 16)  // Rate 1/2, 8 bits
   #define HARQ_MAX_COMBINING    3

   static int8_t s_soft_buffer[HARQ_MAX_COMBINING][HARQ_SOFT_BUFFER_SIZE];
   static int16_t s_combined_buffer[HARQ_SOFT_BUFFER_SIZE];
   ```

3. **Viterbi State:**
   ```c
   #define VITERBI_STATES     64    // 2^(K-1) for K=7
   #define VITERBI_TRACEBACK  35    // 5 * K

   typedef struct {
       int16_t path_metrics[VITERBI_STATES];
       uint8_t survivors[VITERBI_TRACEBACK][8];  // Bit-packed
   } viterbi_state_t;
   ```

4. **Files to Create:**
   ```
   star-rx72n-firmware/lib/rx_fec/
   ├── inc/
   │   ├── rx_fec.h
   │   ├── rx_convolutional.h
   │   └── rx_viterbi.h
   └── src/
       ├── rx_fec.c
       ├── rx_convolutional.c
       └── rx_viterbi.c

   star-rx72n-firmware/lib/rx_harq/
   ├── inc/
   │   ├── rx_harq.h
   │   └── rx_chase_combiner.h
   └── src/
       ├── rx_harq.c
       └── rx_chase_combiner.c
   ```

### Verification Criteria

Before merging to main:

- [ ] Go FEC round-trip encode/decode passes (✅ done)
- [ ] Go HARQ state machine tests pass (✅ done)
- [ ] C FEC produces bit-exact output matching Go for same input
- [ ] C HARQ handles combining and recovery correctly
- [ ] Viterbi decode latency < 1ms on RX72N @ 120 MHz
- [ ] Memory usage within 67 KB budget
- [ ] 100 Hz control loop maintained with HARQ overhead

## References

- [Hybrid ARQ - Wikipedia](https://en.wikipedia.org/wiki/Hybrid_automatic_repeat_request)
- [ARQ vs HARQ - RF Wireless World](https://www.rfwireless-world.com/terminology/arq-vs-harq)
- [Hybrid ARQ - Devopedia](https://devopedia.org/hybrid-arq)
- NASA Convolutional Code Standard: Polynomials 171₈, 133₈
