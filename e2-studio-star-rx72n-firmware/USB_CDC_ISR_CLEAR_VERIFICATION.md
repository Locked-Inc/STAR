# USB CDC ISR Interrupt Clear Procedure Verification

## Overview

**Task**: Verify ISR interrupt clear procedure uses correct method for RX72N
**File**: `libs/rx_usb/src/rx_usb_isr.c`
**Date**: 2026-02-05
**Result**: ✅ **VERIFIED CORRECT**

---

## RX72N Interrupt Clear Requirement

**Critical Requirement from RX72N Manual Ch40 Section 40.2.12:**

> "Cleared by writing 0 (NOT writing 1!)"

**RX72N Interrupt Flag Behavior:**
- **Writing 0** to a flag bit: **CLEARS** the interrupt
- **Writing 1** to a flag bit: **NO EFFECT** (does not set or clear)

**This is opposite of many MCUs** (ARM Cortex-M, some AVR) where writing 1 clears flags.

---

## Verification Method

Searched `rx_usb_isr.c` for all interrupt clear operations:
- INTSTS0 register clears (VBUS, DVST, CTRT, RESM, VALID)
- BRDYSTS register clears (buffer ready flags)
- BEMPSTS register clears (buffer empty flags)

---

## Code Analysis

### INTSTS0 Register Clears

**Found 5 clear operations in rx_usb_isr.c:**

| Line | Function | Code | Flag |
|------|----------|------|------|
| 386 | `internal_handle_vbus_interrupt()` | `usb0()->intsts0 = (uint16_t)~k_usb_intsts0_vbint;` | VBUS interrupt |
| 442 | `internal_handle_dvst_interrupt()` | `usb0()->intsts0 = (uint16_t)~k_usb_intsts0_dvst;` | Device state |
| 459 | `internal_handle_ctrt_interrupt()` | `usb0()->intsts0 = (uint16_t)~k_usb_intsts0_valid;` | VALID flag |
| 501 | `internal_handle_ctrt_interrupt()` | `usb0()->intsts0 = (uint16_t)~k_usb_intsts0_ctrt;` | Control transfer |
| 587 | `internal_handle_resume_interrupt()` | `usb0()->intsts0 = (uint16_t)~k_usb_intsts0_resm;` | Resume |

**Analysis**: ✅ ALL CORRECT

**Procedure Used**:
```c
usb0()->intsts0 = (uint16_t)~k_usb_intsts0_<flag>;
```

**Why This Works**:
1. `k_usb_intsts0_<flag>` is the bit mask (e.g., `0x8000` for bit 15)
2. `~k_usb_intsts0_<flag>` inverts it (e.g., `0x7FFF` for bit 15 = 0)
3. Assignment writes `0x7FFF` to register:
   - Bit 15: **0** (clears the interrupt) ✅
   - Bits 14-0: **1** (no effect on RX72N) ✅

---

### BRDYSTS Register Clears (Buffer Ready)

**Found 1 clear operation (inside loop):**

| Line | Function | Code |
|------|----------|------|
| 532 | `internal_handle_brdy_interrupt()` | `usb0()->brdysts = (uint16_t)~(1U << pipe);` |

**Context**:
```c
for (uint8_t pipe = k_usb_pipe_dcp; pipe <= k_usb_pipe_max; pipe++) {
  if (brdysts & (1U << pipe)) {
    /* Handle pipe event */
    ...
    /* Clear pipe buffer ready flag */
    usb0()->brdysts = (uint16_t)~(1U << pipe);
  }
}
```

**Analysis**: ✅ CORRECT

**Why This Works**:
1. `(1U << pipe)` creates bit mask for pipe (e.g., pipe 2 → `0x0004`)
2. `~(1U << pipe)` inverts it (e.g., `0xFFFB`)
3. Assignment writes `0xFFFB` to register:
   - Bit 2: **0** (clears pipe 2 BRDY flag) ✅
   - All other bits: **1** (no effect) ✅

---

### BEMPSTS Register Clears (Buffer Empty)

**Found 1 clear operation (inside loop):**

| Line | Function | Code |
|------|----------|------|
| 565 | `internal_handle_bemp_interrupt()` | `usb0()->bempsts = (uint16_t)~(1U << pipe);` |

**Context**:
```c
for (uint8_t pipe = k_usb_pipe_dcp; pipe <= k_usb_pipe_max; pipe++) {
  if (bempsts & (1U << pipe)) {
    /* Handle pipe event */
    ...
    /* Clear pipe buffer empty flag */
    usb0()->bempsts = (uint16_t)~(1U << pipe);
  }
}
```

**Analysis**: ✅ CORRECT

**Why This Works**: Same logic as BRDYSTS clears above.

---

## Alternative Methods (NOT USED)

### ✅ Method 1: Assignment with Inverted Bit (CURRENT - CORRECT)

```c
usb0()->intsts0 = (uint16_t)~k_usb_intsts0_vbint;
```

**Pros**:
- Explicit and clear intent
- Works correctly on RX72N (write 0 to clear)
- No read-modify-write (single operation)

**Cons**:
- Writes 1 to all other bits (harmless on RX72N but unusual)

---

### ✅ Method 2: AND with Inverted Bit (ALTERNATIVE - ALSO CORRECT)

```c
usb0()->intsts0 &= ~k_usb_intsts0_vbint;
```

**Pros**:
- More portable (common pattern for clearing bits)
- Preserves other flag states explicitly

**Cons**:
- Requires read-modify-write (3 operations: read, AND, write)
- Slightly slower (negligible on RX72N @ 240 MHz)

**Note**: This would also work correctly but is NOT currently used.

---

### ❌ Method 3: OR with Bit (WRONG - DO NOT USE)

```c
usb0()->intsts0 |= k_usb_intsts0_vbint;  // ❌ WRONG!
```

**Why This Fails on RX72N**:
- Writes 1 to the flag bit
- On RX72N, writing 1 has **NO EFFECT**
- Flag remains set, interrupt keeps firing

**This works on some MCUs** (ARM Cortex-M, where writing 1 clears), but **NOT on RX72N**!

---

## Verification Results

### Summary

| Register | Operations Found | Method Used | Status |
|----------|------------------|-------------|--------|
| **INTSTS0** | 5 clears | `reg = ~bit` | ✅ CORRECT |
| **BRDYSTS** | 1 clear (in loop) | `reg = ~bit` | ✅ CORRECT |
| **BEMPSTS** | 1 clear (in loop) | `reg = ~bit` | ✅ CORRECT |

**Total**: 7 interrupt clear operations, **ALL CORRECT** ✅

---

## Code Correctness Rationale

The ISR code uses the pattern:

```c
register = (uint16_t)~flag_bit;
```

This is **CORRECT** for RX72N because:

1. **Clears the target flag**: Writing 0 to the flag bit clears it (RX72N requirement)
2. **No side effects**: Writing 1 to other bits has no effect on RX72N
3. **Single operation**: No read-modify-write race conditions
4. **Explicit intent**: Clear and obvious what the code is doing

**Alternative patterns that would also work**:
- `register &= ~flag_bit` (read-modify-write, preserves other flags)

**Patterns that would FAIL on RX72N**:
- `register |= flag_bit` (writes 1, has no effect on RX72N)
- `register = flag_bit` (writes 1 to flag, 0 to others - clears wrong bits!)

---

## Blocking Issue Status

**Original Finding from Phase 1 Task 1.3**:

> **Issue #2: Interrupt Clear Procedure Critical Finding**
> **Severity**: CRITICAL
> **Problem**: RX72N clears interrupts by writing 0, not 1 (opposite of many MCUs)
> **Priority**: **BLOCKING** - Must verify before bulk transfers

**Verification Result**: ✅ **ISSUE RESOLVED**

The ISR code was already using the correct procedure. No changes needed.

---

## Recommendations

### No Changes Required ✅

The current ISR interrupt clear procedure is **100% correct** for RX72N.

### Optional Enhancement (Low Priority)

Add inline comments explaining the RX72N-specific clear behavior:

```c
/* Clear VBUS interrupt flag (RX72N: write 0 to clear, write 1 = no effect) */
usb0()->intsts0 = (uint16_t)~k_usb_intsts0_vbint;
```

This would help future maintainers understand why we use `= ~bit` instead of `|= bit`.

### Documentation Update

Add note to `rx_usb_isr.c` file header explaining RX72N interrupt clear semantics:

```c
/**
 * @note RX72N Interrupt Clear Procedure
 * RX72N USB interrupts are cleared by writing 0 (NOT 1!). This is opposite
 * of many ARM Cortex-M MCUs where writing 1 clears flags. The ISR uses
 * the pattern `register = ~flag` which writes 0 to the target bit (clears)
 * and 1 to all other bits (no effect on RX72N).
 */
```

---

## Conclusion

**Status**: ✅ **VERIFIED CORRECT**

All 7 interrupt clear operations in `rx_usb_isr.c` use the correct procedure for RX72N:
- INTSTS0 clears: 5 operations ✅
- BRDYSTS clears: 1 operation ✅
- BEMPSTS clears: 1 operation ✅

**No code changes required**. The ISR was already implemented correctly.

**Blocking Issue #2 from Phase 1**: ✅ **RESOLVED**

Ready to proceed to Phase 2 after adding missing pipe bit definitions.

---

**Verification Completed**: 2026-02-05
**Verified By**: Manual code review + grep analysis
**Confidence Level**: HIGH (100% - all clear operations checked)
