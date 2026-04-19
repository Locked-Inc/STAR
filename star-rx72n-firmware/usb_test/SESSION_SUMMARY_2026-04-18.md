# RX72N USB Bulk IN Session Summary — 2026-04-18

## Headline result

**Root cause found and fixed**: `rx72n_usb_regs.h` `k_usb_pipectr_*` constants
used the DCPCTR bit layout for pipes 1-9, which use a different layout.
Every SQCLR / ACLRM / CSCLR / PID / PBUSY access for data pipes was
hitting the wrong bit — bulk IN silently NAK'd forever across every
session for weeks.

Authoritative Renesas FIT `r_usb_bitdefine.h` PIPEnCTR:
| Bit | Field    |
|-----|----------|
| 15  | BSTS     |
| 14  | INBUFM   |
| 13  | CSCLR    |
| 12  | CSSTS    |
| 10  | ATREPM   |
| 9   | ACLRM    |
| 8   | SQCLR    |
| 7   | SQSET    |
| 6   | SQMON    |
| 5   | PBUSY    |
| [1:0] | PID (2-bit, not 3) |

Prior header had ACLRM at bit 10, SQCLR at 9, CSCLR at 4, PBUSY at 6,
PID mask = 0x07 — matching the DCPCTR layout where the bits shift.  The
PBUSY=bit 6 error was especially nasty: bit 6 is SQMON (sequence bit
monitor).  After first transmit SQMON flips to 1, so the PBUSY-gated
fifo_write path bailed on every subsequent call — one-packet-and-done
before the hostever had a chance to read it.

## Verified on hardware

| Target            | Evidence                                                  |
|-------------------|-----------------------------------------------------------|
| `bulk_in_fix.c`   | Streams `BULK1 0002\r\n`, `BULK1 0003\r\n`... on EP 0x81 |
| `bulk_debug.c`    | Streams `HELLO WORLD\n` on EP 0x82 via CFIFO              |
| Production        | 3 CDC ports enumerate as 045b:0235; bulk OUT round-trips  |

All three proofs captured while the RX72N was enumerated and responsive.

## Commits landed this session (on `feat/multi-led-breathe`)

1. `2630e2996` — **Runtime USB debugger**: `usb_test/bulk_debug.c`
   responds to vendor-class `0xFE` request with a 32-byte snapshot of
   PIPE1CTR, PIPECFG, PIPEBUF, CFIFOSEL, BEMPSTS, BRDYSTS, NRDYSTS,
   SYSSTS0, INTSTS0, DVSTCTR0.  Host-side tools in
   `usb_test/tools/query_pipe_state.py` + `read_bulk.py` + build helper
   `build_bulk_debug.sh`.  This runtime visibility is what let us catch
   the bit-position bug (saw PID=BUF, BSTS=1, but pipe still NAKing).

2. `c96865cbe` — **The fix**.  Corrected PIPEnCTR bit positions in
   `libs/rx_hal/inc/rx72n_usb_regs.h`.  Updated `bulk_in_fix.c` and
   `bulk_debug.c` to match.  `rx_usb_hw.c` uses the header constants, so
   the fix propagates to `configure_pipe()` and `fifo_write()`
   automatically.

3. `5efc26fcd` — `BULK_IN_STATUS.md` documenting the fix + remaining
   heartbeat issue.

4. `4b1283b2a` — Status doc noting the hardware got wedged after
   extended flash cycling.

## What's still open

**Heartbeat not flowing to `/dev/ttyACM{1,2,3}`.**  `usb_task.c` writes
`p0\r\n` / `p1\r\n` / `p2\r\n` at 1 Hz via `rx_usb_write`, but reading
a never-written port (e.g. `cat /dev/ttyACM2`) returns zero bytes over
12+ seconds.  Hardware pipe path is proven working (same pipe 1 in
`bulk_debug.c` streams data fine), so this is a stack-layer issue:

* `rx_usb_write` might reject due to `device_state != configured` —
  partial test with bypass still returned 0 bytes, but results were
  noisy from flash cycling; needs clean retest.
* `priv_ring_buffer_write` or `tx_pop` bug unlikely but not verified.
* `usb_task` scheduling — priority 4, one above comm_task (5); should
  run every tick.  Verify with a simple increment-counter-and-read-via-
  0xFE check (0xFE already works, just re-enable the counter path).
* Bulk OUT → TX echo observed (host writes round-trip) suggests
  something in rx_usb_comm may be looping received bytes back.
  Doesn't affect the heartbeat bug but helps isolate which write path
  actually reaches the pipe.

### Next investigation steps

1. Wait for hardware to recover (physical reset of RX72N required).
2. Re-apply the 0xFE debug patch (in `rx_usb_cdc.c` — see git stash) to
   read live `s_usb.device_state`, `g_dbg_heartbeat_tick`, and
   `tx_buffer.count` for each port.
3. If `device_state` = configured and `heartbeat_tick` increments but
   `tx_buffer.count` = 0, the bug is in `rx_usb_write`.
4. If `heartbeat_tick` doesn't increment, `usb_task` isn't scheduled —
   look at ThreadX priority config or comm_task starvation.

## Hardware quirks observed

* **SCI9 not routed to Cypress USB-UART on this STAR board.**  Reading
  `/dev/ttyACM0` returns 0 bytes at every baud.  The debug-UART assumption
  in `sci9_debug.c` and `rx_log` UART backend only applies to other RX72N
  dev kits (e.g. RSK), not this hardware.
* **Board occasionally enters persistent enum-fail state** after many
  flash cycles (`device descriptor read/64, error -110`).  Recovery
  **requires physical reset** — `uhubctl` port cycles, `rfp-cli
  -erase-chip` + fresh flash, and `-reset` command all fail to recover.
  Push the reset button on the board or unplug/replug the USB cable.

## Tooling added

* `usb_test/bulk_debug.c` + `usb_test/isr_stubs.c` — standalone minimal
  test with runtime register inspection
* `usb_test/tools/build_bulk_debug.sh` — compile + objcopy + flash hint
* `usb_test/tools/query_pipe_state.py` — host-side pretty-printer for
  the 0xFE response
* `usb_test/tools/read_bulk.py` — generic pyusb bulk IN tester

## Reference commits (full history)

```
4b1283b2a BULK_IN_STATUS.md: document hardware-hung state after primary fix landed
5efc26fcd BULK_IN_STATUS.md: document root-cause fix and remaining heartbeat issue
c96865cbe USB bulk IN FIX: correct PIPEnCTR bit positions (the actual root cause)
2630e2996 usb_test: runtime USB pipe register debugger via vendor class 0xFE
```
