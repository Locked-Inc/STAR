# RX72N USB0 Bulk IN Bring-up -- IN PROGRESS

**Status (2026-04-18)**: 3 CDC-ACM ports enumerate cleanly as `045b:0235`
on every fresh power-cycle, but **bulk IN never transmits bytes**. Host
submits 16 IN URBs per port on opening `/dev/ttyACM{0,1,2}`; all 16 stay
at `-115 EINPROGRESS` indefinitely and eventually cancel with `-2 ENOENT`
when the port closes. Zero bytes ever reach the host.

**Latest (2026-04-18, commits `d465ecd07..e39114171`):**
* Cleanup landed: removed pre-`tx_kernel_enter` raw-register pokes from
  `usb_task`, removed AD2 trace-beacon GPIO toggling from
  `internal_handle_set_configuration`, replaced 64-byte 'Z' spam with a
  1 Hz `"p0\r\n"` / `"p1\r\n"` / `"p2\r\n"` heartbeat per port.
* `rx_usb_hw_fifo_write` simplified to mirror `bulk_in_fix.c`:
  single-buffer bulk IN (DBLB removed for IN), BCLR before each write,
  no PID=NAK toggle (configure_pipe arms PID=BUF and we leave it),
  CFIFOSEL ISEL=1 for IN data pipes (matches the working raw repro).
* PIPEMAXP cached at configure-time so the write hot-path doesn't touch
  `PIPESEL` (writes to PIPESEL silently rebound CURPIPE on this chip).
* Port 2 (log) keeps both bulk IN and bulk OUT advertised so cdc_acm
  doesn't reject the data interface with `-EINVAL`; host writes are
  silently consumed at the device side.

Even with all of the above, IN URBs sit at `-EINPROGRESS` forever -- the
silent-NAK behaviour is unchanged from the previous milestone.  The
1-byte raw-register repro that previously emitted `0x5A` bytes also no
longer transmits, even with `rx_usb_hw.c` reverted to the prior
checkpoint -- strongly suggesting that what we observed earlier was a
transient state, not a fix that the cleanup regressed.

**Five further fixes landed (commits `c217516eb..f73e0dd1d`):**

1. **rx_log_usb_putc/puts ISR-safe** (`rx_log_usb.c`).  Switched from
   `tx_mutex_get(TX_WAIT_FOREVER)` to `TX_NO_WAIT` because these
   functions are reachable from USB ISR context via `rx_log_debug` in
   the SETUP handlers.  Blocking acquires from ISR are undefined in
   ThreadX and were observed to hang `SET_CONTROL_LINE_STATE`'s CCPL
   write so the host's open-time class request never completed.
2. **Drop `rx_log_debug` calls inside ISR-context SETUP handlers**
   (`rx_usb_cdc.c`: `handle_set_address`, `handle_set_line_coding`,
   `handle_set_control_line_state`).  Even with TX_NO_WAIT the call
   re-enters `rx_usb_write` -> `rx_usb_hw_fifo_write` (writing the log
   line to port 2) which does its own FRDY waits and CFIFOSEL changes
   on the same shared CFIFO that DCP is mid-CCPL on.
3. **Mask USB0 USBI IRQ during `rx_usb_hw_fifo_write` CFIFO sequence**
   (`rx_usb_hw.c`).  Save+disable ICU IER[4] bit 4 around the
   PBUSY-check / CFIFOSEL switch / FRDY wait / byte-write / BVAL
   block; restore on every exit path.  Prevents a BRDY/BEMP/CTRT
   firing mid-write from re-entering the ISR and rebinding
   CFIFOSEL.CURPIPE to a class-request pipe.

4. **Fix PIPEnCTR bit positions in rx72n_usb_regs.h**.  The header
   was using DCPCTR layout (PID at bits [1:0], PBUSY at bit 5,
   SQCLR at 8, ACLRM at 9, CSCLR at 13).  PIPEnCTR for n=1..9 has
   different field positions: PID at [2:0] (3 bits), PBUSY at 6,
   SQMON at 7, SQSET at 8, SQCLR at 9, ACLRM at 10, CSCLR at 4,
   INBUFM at 13.  Old configure_pipe was setting INBUFM=1 where it
   intended CSCLR -- left bulk-IN pipes in IN-direction-completion
   BRDY mode instead of buffer-ready BRDY mode, blocking any host
   stack that waits for BRDY.

5. **Route bulk pipe FIFO writes through D0FIFO** (`rx_usb_hw.c`)
   instead of CFIFO so they don't contend with DCP class-request
   traffic.  CFIFO is shared between DCP and data pipes;
   bulk_in_fix.c works because once it reaches SET_CONFIGURATION
   the DCP goes silent and bulk owns CFIFO uncontested.  Our
   firmware keeps cdc_acm class requests flowing forever
   (SET_LINE_CODING / SET_CONTROL_LINE_STATE / GET_LINE_CODING)
   which previously interleaved with bulk IN BVAL commits on the
   same FIFOSEL/FIFOCTR pair.

After all five fixes plus the pre-kernel spin retuned from 80M to 1M
to 10M iterations (`main.c`):

* All three CDC interfaces enumerate cleanly as ttyACM0..2 (or
  ttyACM1..3 depending on Cypress UART order) on every fresh
  power cycle (when the Pi 5 USB hub state isn't in its
  intermittent-dead recovery window).
* SET_LINE_CODING completes (1-2ms) -- handler reduced to just CCPL,
  no longer reads from DCP FIFO inside the SETUP context.
* SET_CONTROL_LINE_STATE still **never completes**.  Each URB hangs
  for 5s then cancels with -2 ENOENT.  cdc_acm tolerates the
  failure (binds the port anyway) so port-open takes ~5s per port
  ~= 15s total for all three.
* Bulk IN URBs still **never complete**.  All 48 IN URBs (16 per
  port × 3 ports) sit at -EINPROGRESS for the lifetime of the open
  and cancel on close.

The remaining gap appears to be in the DCP CCPL sequencing for class
requests -- standard requests (SET_ADDRESS, SET_CONFIGURATION) complete
fine with the same `dcpctr |= CCPL` write, but class requests don't.

Things tried and ruled out as the culprit (each tested independently):
* Atomic `(dcpctr & ~PID_MASK) | PID_BUF | CCPL` write instead of
  `dcpctr |= CCPL`.
* CCPL set BEFORE state mutation in handler vs after.
* Replace CCPL with `dcpctr |= PID_STALL` to force a STALL response
  the host should observe -- still no completion (so the issue
  isn't "wrong PID value", it's "DCP doesn't respond at all").
* BCLR the CFIFO with CURPIPE=0 before CCPL.
* Clear CTRT at the START of `internal_handle_ctrt_interrupt`
  rather than the end, so a wr_nd -> wr_status transition during
  the handler can re-arm CTRT.  This BROKE enumeration entirely.
* Disable USBI ICU vector and rely on usb_task polling.  This
  also broke enumeration -- pre-kernel spin services initial
  enumeration but post-kernel SETUP responsiveness suffers
  without the IRQ.

Still untried (next-iteration candidates):
* `INBUFM` bit in PIPECFG for IN data pipes -- now that the header
  has it at the right position (bit 13), explicitly clear it in
  configure_pipe; the previous reset value behaviour wasn't checked.
* Switch DCP class-request handling to a deferred-event model
  (set a flag in the ISR, process from a thread) so the actual
  CCPL write happens outside the ISR critical section.  FIT does
  this -- usb_pstd_request_event_set queues; the application
  processes from a peripheral driver task.

## Correction (2026-04-18): earlier "HW defect" conclusion was wrong

An earlier AD2 scope capture suggested D- was stuck at 10+ V (impossible)
and concluded the silicon was defective.  **That reading was a bad
solder joint on the AD2 Ch2 probe, not a board defect.**  After the
probe was resoldered, D- showed clean 0 ↔ 3.3 V USB FS signaling,
matching D+.  Retain the firmware-side conclusions below but ignore
any claim that the silicon cannot drive USB FS -- it drives it fine
(NAK responses are clean, eye is open ±3.1 V differential, edges
10-30 ns rise/fall).

## 2026-04-18 bus-level capture proves device NAKs every IN (AD2 on D+/D-)

Hooked a Digilent Analog Discovery 2 to D+ (DIO1) and D- (DIO0) with
common ground, sampled at 100 MS/s, and decoded the bus traffic
while the host polled bulk IN on /dev/ttyACM1.

Over a 30-second capture with cdc_acm issuing IN URBs continuously:
* 849 IN tokens from host (PID 0x69)
* 392 NAK responses from device (PID 0x5A)
* **0 DATA0/DATA1 packets from device (PID 0xC3 / 0x4B)**
* 37 SOF packets from host (PID 0xA5) -- bus is healthy

Every IN token gets a prompt NAK, confirming the device silicon is
alive and responding within USB spec timing -- just always with NAK.
The device's pipe 1 buffer is empty from the bus's perspective 100%
of the time.

Tried to force the issue by writing 'A' to pipe 1 via raw register
access from `main.c`'s pre-kernel spin (bypasses rx_usb_write ->
rx_usb_hw_fifo_write entirely, matches `usb_test/bulk_in_fix.c`
cfifo_write_current exactly).  AD2 capture: still 850/440 IN/NAK,
zero DATA.  CPU writes aren't loading the buffer even with
verified-correct PIPEnCTR bit positions and bulk_in_fix.c's proven
CFIFO sequence.

## 2026-04-18 board-level finding: bulk IN broken in `bulk_in_fix.c` too

Flashed the standalone `bulk_in_fix.mot` (vendor-class single-bulk-IN
test, 1209:0002) and tried to read EP 0x81 via libusb-python.  Read
timed out after 2s with `-EIO -110` -- the device enumerates as
1209:0002 cleanly, but bulk IN never transmits a byte.

AD2 bus-level capture while libusb reads for 15s:
* 197 IN tokens from host
* 146 NAK responses from device
* **0 DATA packets from device**

`bulk_in_fix.c` uses verified-correct PIPEnCTR bit positions
(PID mask 0x07, ACLRM bit 10, SQCLR bit 9 -- the layout the shared
header was fixed to match this session), single-buffer bulk IN
PIPECFG=0x4011, PIPEBUF=0x0008 (slot 8, 64B).  These are the
identical values bulk_in_fix.c previously used when it reportedly
transmitted "BULK1 %04d\r\n" payloads.

Earlier I concluded this was a silicon defect, but that was based
on the bad-solder Ch2 reading.  With the probe repaired and clean
D+/D- waveforms confirmed, the actual conclusion is:

**The device generates clean USB FS output for NAK handshakes but
never produces DATA packets.  This is a firmware issue -- pipe 1's
buffer isn't getting filled/armed in a way the transceiver picks up
on the next IN poll.**

Firmware-side suspects still in play:
* D0FIFOSEL misprogramming -- ISEL bit 5 exists only on CFIFOSEL;
  on D0FIFOSEL bit 5 is reserved/DMA-related.  Writing 1 there may
  have been misconfiguring the D0FIFO bank so our BVAL commits
  never reached the transceiver.  Fixed in rx_usb_hw.c: only set
  ISEL when CURPIPE=DCP (CFIFO).
* PIPECFG / PIPEBUF transaction ordering inside configure_pipe.
* BEMPSTS clear timing (FIT clears AFTER BVAL; we were clearing
  before).

Hypotheses now in play (unverifiable without scope/JTAG):
1. **RX72N USB0 silicon errata** that affects bulk IN BVAL commits
   on this specific chip lot or revision.  `r01us0263ej0170_rx72n`
   errata sheet would confirm.
2. **HOCO PLL / 48 MHz UCK clock instability** -- `bulk_in_fix.c`
   includes the HOCO+PLL bring-up sequence from `hoco_pid_fix.c`,
   so if that path produced a marginal 48 MHz clock, bulk IN PHY
   timing might be off enough to NAK every IN token even though
   enumeration (which uses the same clock) works.  Cypress
   USB-UART `BUSDETECT` OTP issue documented in the board schematic
   suggests USB clock/PHY area is fragile on this hardware.
3. **Board-level VBUS / D+/D- signal integrity issue** intermittent
   enough that DCP control transfers (short frames, NAK-tolerant)
   work most of the time but bulk transfers (longer frames,
   sequence-number sensitive) silently fail.

Recommend: scope D+/D- on the RX72N USB0 connector while the host
issues IN tokens, confirm the device is actually NAKing (DATA0/1
PID never appears), then walk the RX72N schematic for VBUS bypass
caps / pull-up resistor / decoupling around the USB0 PHY.

For now: 3-port enumeration is solid; bulk IN bring-up is blocked
on hardware investigation, not firmware.

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
   - **CFIFO contention with DCP class requests.**  `bulk_in_fix.c` is
     a vendor-class device with no DCP traffic past SET_CONFIGURATION,
     so the bulk pipes own CFIFO uncontested.  We have ongoing
     SET_LINE_CODING / SET_CONTROL_LINE_STATE / GET_LINE_CODING from
     cdc_acm using DCP via the same CFIFO.  Try routing bulk IN
     through `D0FIFO` and bulk OUT through `D1FIFO` so DCP and bulk
     don't share a FIFO port.  FIT supports this via the `pipemode`
     argument to `usb_pstd_write_data`.
   - `INBUFM` bit in `PIPEnCTR` may need explicit handling for IN pipes.
   - `BFRE` bit in PIPECFG (receive-mode buffer) -- we leave it at 0;
     verify FIT does the same for IN pipes (it does, but reconfirm for
     RX72N USB0 specifically).
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
