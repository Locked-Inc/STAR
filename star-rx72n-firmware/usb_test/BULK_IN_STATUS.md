# RX72N USB0 Bulk IN Bring-up -- IN PROGRESS

**Status**: 3 CDC-ACM ports enumerate cleanly as `045b:0235` but **bulk IN
never transmits bytes**. Host submits 16 IN URBs per port on opening
`/dev/ttyACM{1,2,3}`; all 16 stay at `-115 EINPROGRESS` indefinitely and
eventually cancel with `-2 ENOENT` when the port closes. Zero bytes ever
reach the host.

Companion to `USB_BRINGUP_STATUS.md` (which documented the earlier 10-bug
enumeration bring-up).

---

## Symptom (usbmon capture)

```
S Bi:1:026:7 -115 128 <          -- 16 bulk IN URBs submitted on EP 0x87
S Bi:1:026:7 -115 128 <             (port 2 / k_usb_port_log / ttyACM3)
... [15 more]
C Bi:1:026:7 -2 0                -- all 16 cancel with -ENOENT on close
```

Identical pattern on port 0 (EP 0x81 / ttyACM1 / pipe 1) and port 1
(EP 0x84 / ttyACM2 / pipe 4). Host's IN tokens are being NAK'd by the
RX72N forever -- which means firmware *is* responding to the token, but
the pipe's DPRAM buffer never contains data for transmission.

CDC class-specific control transfers over DCP *also* hang past
enumeration -- `SET_CONTROL_LINE_STATE(DTR=1, RTS=1)` submitted on open
cancels with `-2` without ever completing, even though the initial
`GET_DESCRIPTOR` + `SET_ADDRESS` + `SET_CONFIGURATION` sequence during
enumeration works fine.

---

## What Works

| Path | Status |
|------|--------|
| Enumeration (DCP GET_DESCRIPTOR, SET_ADDRESS, SET_CONFIGURATION) | [OK] passes |
| 3 ttyACM ports appear, cdc_acm binds to all 6 CDC interfaces | [OK] |
| Device descriptor (`lsusb -v`) returns correct 207-byte config | [OK] |
| Linux submits bulk IN URBs on all 3 endpoints (0x81/0x84/0x87) | [OK] |
| 2000+ unit tests pass (`tests/` directory) | [OK] |

---

## What Doesn't

| Path | Status |
|------|--------|
| Bulk IN transmission (device -> host) | [FAIL] 0 bytes ever transmit |
| Bulk OUT reception (host -> device) | [FAIL] URBs cancel without completing |
| CDC SET_CONTROL_LINE_STATE after SET_CONFIGURATION | [FAIL] hangs, cancels |
| CDC SET_LINE_CODING | [FAIL] hangs, cancels |
| Cypress CY7C65213A UART bridge at `/dev/ttyACM0` (SCI9 TX debug) | [FAIL] silent at every baud rate -- schematic notes BUSDETECT OTP issue |

---

## Recent Attempts (commits `7186be786..2f7770e87`, 2026-04-16..2026-04-18)

### Fixed
1. **PIPEnCTR bit positions** (`14419ed88`) -- were using DCPCTR layout
   (SQCLR@8 / SQSET@7 / ACLRM@9 vs DCPCTR's SQSET@7 / SQCLR@8).
2. **FIT `usb_cstd_pipe_init` alignment** (`416140a1b`) -- mirror the exact
   sequence from `r_usb_basic` v1.44:
   ```
   1. Disable BRDY/NRDY/BEMP interrupts for pipe
   2. Force PID=NAK
   3. PIPESEL=n, write PIPECFG/PIPEMAXP/PIPEPERI; PIPESEL=0
   4. Pulse SQCLR, ACLRM, CSCLR on PIPEnCTR
   5. Clear BRDYSTS/NRDYSTS/BEMPSTS for pipe
   6. Set PID=BUF
   ```
3. **`usb_pstd_send_start` per-packet PID wrap** (`8c6eb90ad`) -- bracket
   every data-pipe FIFO write with:
   ```
   PID=NAK -> spin until PBUSY=0
     -> clear BEMPSTS/BRDYSTS for pipe
     -> CFIFOSEL clear-wait-set to CURPIPE=n, ISEL=0, MBW=8
     -> wait FRDY -> chunked byte writes -> BVAL per chunk
   PID=BUF
   ```
4. **Drop BCLR before every data-pipe write** (`8c6eb90ad`) -- FIT never
   BCLRs mid-transfer on data pipes; issuing it between bulk IN packets
   throws away the in-flight double-buffered chunk.
5. **ICU vector 36 (USB0_USBI) enable** (`8c6eb90ad`) -- `main.c` inline
   USB attach now writes `ICU.IPR[36] = 12` and sets `ICU.IER[4] bit 4`
   so BRDY/BEMP/CTRT interrupts fire directly. Without this, post-
   `tx_kernel_enter` servicing relied solely on `usb_task` polling at the
   ThreadX tick rate (10 ms), which was too slow for CDC control
   transfers and BEMP refills.
6. **PIPEBUF DPRAM allocation for pipes 1-9** (`2f7770e87`) -- RX72N USB0
   ships with `PIPEBUF=0` for each pipe, meaning every data pipe shadows
   the DCP's slot 0-7 region. Writes to a data pipe's CFIFO followed by
   BVAL silently drop because the hardware has no DPRAM area in which to
   land them. Layout chosen:
   ```
   pipe 1 (p0 bulk IN)  : BUFNMB=8,  BUFSIZE=0 + DBLB=1  (slots 8-9)
   pipe 2 (p0 bulk OUT) : BUFNMB=10, BUFSIZE=0 + DBLB=1  (slots 10-11)
   pipe 3 (p0 int IN)   : BUFNMB=12, BUFSIZE=0           (slot 12)
   pipe 4 (p1 bulk IN)  : BUFNMB=14, BUFSIZE=0 + DBLB=1  (slots 14-15)
   pipe 5 (p1 bulk OUT) : BUFNMB=16, BUFSIZE=0 + DBLB=1  (slots 16-17)
   pipe 6 (p1 int IN)   : BUFNMB=18, BUFSIZE=0           (slot 18)
   pipe 7 (p2 bulk IN)  : BUFNMB=20, BUFSIZE=0 + DBLB=1  (slots 20-21)
   pipe 8 (p2 bulk OUT) : BUFNMB=22, BUFSIZE=0 + DBLB=1  (slots 22-23)
   pipe 9 (p2 int IN)   : BUFNMB=24, BUFSIZE=0           (slot 24)
   ```
7. **1 Hz heartbeat in `usb_task`** (`2f7770e87`) -- writes `"p0"`/`"p1"`/
   `"p2"` to each CDC port every second so `cat /dev/ttyACM{1,2,3}`
   reveals whether the bulk IN data path is flowing now that PIPEBUF is
   programmed. Not intended to be permanent; revert once the data path
   is proven.

### Not fixed

- The PIPEBUF allocation in (6) was written and committed but the board
  was not in a flashable state at the time (see "Hardware state" below),
  so its effect on bulk IN has **not yet been verified**.

### Exhausted without success

| Approach | Reason ruled out |
|----------|------------------|
| `CFIFOSEL.ISEL=1` for data pipes | FIT uses `ISEL=0`; setting 1 forces wrong FIFO access mode |
| Single-buffer bulk (no DBLB) | FIT always sets DBLB on bulk IN/OUT |
| Per-chunk PID save/restore inside `rx_usb_hw_fifo_write` | PID wrap belongs at the per-transfer level, not per-chunk |
| BCLR before every bulk IN write | Kills in-flight double-buffered packet |
| Raw register writes matching `usb_test/bulk_in_fix.c` | That minimal test *also* fails to transmit -- confirms issue is at hardware programming level, not `libs/`-specific |
| Clearing CFIFOSEL.CURPIPE single-shot (no wait) | Hardware needs clear-wait-set sequence |

---

## Probable Root Cause

Pre-PIPEBUF fix (everything before `2f7770e87`): pipes 1-9 default to
`BUFNMB=0, BUFSIZE=0` which overlays the DCP's DPRAM region. Every
bulk IN BVAL writes into the DCP's buffer space which is never read out
on an IN token for a data pipe, so the bulk IN endpoint answers the
host's IN token with NAK every time because its own buffer is empty.

The FIT library itself never writes `PIPEBUF` for `USB0`/`IP0` -- the
`hw_usb_write_pipebuf` call is guarded by
`#if defined(BSP_MCU_RX64M) || defined(BSP_MCU_RX71M)` and further by
`#if USB_CFG_USE_USBIP == USB_CFG_IP1`. On those chips the FIT assumption
that USB0 has a fixed internal buffer mapping may be correct; on RX72N
USB0 it is **not** -- pipes need explicit PIPEBUF programming or they
silently fail on transmission. This matches the symptom we see.

The `2f7770e87` commit adds that programming. If the hypothesis is right,
bulk IN should produce `"p0\r\n"`/`"p1\r\n"`/`"p2\r\n"` on each port at
1 Hz once flashed.

---

## Hardware state (2026-04-18)

The Pi 5's USB bus is in a degraded state from repeated soft-reset pokes
during debug:

- **E2 Lite JTAG** (`045b:82a0` at `1-1.4`): intermittently stuck. When
  visible, `lsusb -v` returns `Resource temporarily unavailable (11)` and
  `rfp-cli` fails with `E3000201: Cannot find the specified tool`. Power-
  cycling the hub port via `uhubctl -l 1-1 -p 4 -a cycle` did **not**
  recover it; the board appears to have its own 5 V rail that keeps the
  E2 Lite's upstream side alive regardless of hub VBUS state.
- **RX72N USB0** (`045b:0235` at `1-1.1`): intermittently dead. Kernel
  shows `device descriptor read/64, error -110` on every retry. The
  firmware is running (earlier flashes succeeded) but the USB0 peripheral
  stopped responding after the bus was disturbed. IWDT is 2048 ms but
  `watchdog_monitor_task` keeps feeding it so no automatic reset fires.
- **Cypress CY7C65213A UART bridge** (`04b4:0003` at `1-1.2`): same
  pattern -- visible after hub power cycles, then drops when any probing
  starts.
- **Digilent FT232H** (`0403:6014` at `1-1.3`): unrelated module that
  recovers fine because it has its own power.

Only way to recover without physical intervention observed so far: wait
~5-30 min after heavy poking, then sometimes `uhubctl` port-cycle will
let the VIA hub re-discover 1-1.1/1-1.2/1-1.4 cleanly. Not reliable.

**Flashing tool invocation** (once E2 Lite is responsive):
```
sudo /usr/local/bin/rfp-cli -d RX72N -t e2l -if fine \
    -run -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -noprogress \
    -a star-rx72n-firmware/build/star-rx72n-firmware.mot
```
(The wrapper runs via `box64 + DOTNET_EnableDiagnostics=0` because the
Renesas Linux build is x86_64 .NET Core 3.1.)

---

## Debug avenues that aren't available

- **UART debug (SCI9 via Cypress bridge at `/dev/ttyACM0`)** -- completely
  silent at every baud rate. Likely the `BUSDETECT` OTP flag on the
  Cypress CY7C65213A as documented in the board schematic. Cannot be
  changed without a Cypress programming jig.
- **JTAG GDB** -- no GDB-speaking tool for the E2 Lite FINE protocol is
  available on Linux. `rfp-cli` only does flash programming.
- **SWD / in-circuit ICE** -- pins not broken out.

This is why every fix above is "stare at FIT source and guess" rather
than "step through the ISR and watch registers".

---

## Next iteration (when hardware returns)

1. Flash `build/star-rx72n-firmware.mot` (commit `2f7770e87`).
2. `sudo chmod a+rw /dev/ttyACM{1,2,3}` then
   `timeout 5 cat /dev/ttyACM3`.
3. Expect: `p2\r\np2\r\np2\r\np2\r\np2\r\n` (one per second).
4. If bytes appear on ACM1 (`p0`) and ACM2 (`p1`) and ACM3 (`p2`): the
   PIPEBUF hypothesis is correct; remove the heartbeat, re-wire
   `star_gateway` traffic through `k_usb_port_proto`, verify proto frames
   round-trip. Close task #21 (`Verify bulk IN data transmits`).
5. If still silent: deeper investigation needed. Candidates in decreasing
   plausibility:
   - `INBUFM` bit in `PIPEnCTR` may need explicit handling for IN pipes.
   - `BFRE` bit in PIPECFG (receive-mode buffer) -- we leave it at 0;
     verify FIT does the same for IN pipes (it does, but reconfirm for
     RX72N USB0 specifically).
   - Double-buffer transition race: maybe BVAL on the *second* buffer
     while the first is still in flight behaves differently than a
     single-buffered IN on RX72N.
   - Switch bulk IN to `D0FIFO` instead of `CFIFO` -- FIT supports both;
     maybe CFIFO has shared-port contention we're hitting.
   - Silicon errata RX72N USB0 data pipes -- check `r01us0263ej0170_rx72n`
     errata sheet if accessible.

## Open tasks

- **#21** -- `Verify bulk IN data transmits` (pending, blocked on
  hardware recovery + flash).

## Relevant files

- `libs/rx_usb/src/rx_usb_hw.c`:`rx_usb_hw_fifo_write` (FIT send_start
  wrap), `rx_usb_hw_configure_pipe` (PIPEBUF allocation).
- `libs/rx_usb/src/rx_usb.c`:`internal_trigger_tx_if_idle` (kicks
  `rx_usb_cdc_handle_bulk_in` on first write).
- `libs/rx_usb/src/rx_usb_cdc.c`:`rx_usb_cdc_handle_bulk_in`,
  `internal_enable_interrupts_and_notify` (enables BRDY/BEMP after
  SET_CONFIGURATION).
- `libs/rx_usb/src/rx_usb_isr.c`:`internal_handle_bemp_interrupt`,
  `internal_handle_brdy_interrupt`, `internal_handle_ctrt_interrupt`.
- `src/main.c`: inline USB attach + ICU IER[4] enable pre-`tx_kernel_enter`.
- `src/tasks/usb_task.c`: priority-4 polling loop + 1 Hz heartbeat.
- `usb_test/bulk_in_fix.c`: minimal standalone repro (also fails -- used
  to prove the issue is lower than `libs/`).
- Renesas FIT reference: `/tmp/r_usb_basic/src/driver/r_usb_plibusbip.c`
  (`usb_pstd_send_start`, `usb_pstd_write_data`,
  `usb_pstd_set_pipe_table`, `usb_pstd_get_pipe_buf_value`).
