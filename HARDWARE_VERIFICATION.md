# Hardware Verification Runbook -- Post-USB0-Purge

This branch replaces the RX72N's native USB0 interface with a single UART path
through the CY7C65213 USB-UART bridge. This runbook walks through end-to-end
verification on real hardware using the Renesas E2 Lite debugger and the
UART-to-USB bridge connected to a development machine (acting as the Pi5
stand-in for the gateway side).

## Hardware setup

1. **E2 Lite** -> RX72N JTAG header (debug + flash).
2. **CY7C65213 USB-UART** (already on the PCB) -> USB-C of the dev machine.
3. No other USB cable to the board's USB0 USB-C is needed. That path is dead.
4. Power the board.

## Host-side prerequisites (dev machine)

```bash
# Expect the Cypress bridge to enumerate as /dev/ttyUSB0 on Linux,
# /dev/cu.usbmodemXXXX on macOS.
ls -l /dev/ttyUSB* /dev/cu.usb* 2>/dev/null

# If on Linux and it doesn't show, check the cypress_m8 driver:
dmesg | tail | grep -i cypress
```

## Flash the firmware

Use e^2 studio or `rfp-cli` over E2 Lite:

```bash
# Example (rfp-cli must be configured for RX72N + E2 Lite):
rfp-cli -device RX72N -connect -port FINE -interface E2L \
        -write star-rx72n-firmware.mot
```

Or flash from within e^2 studio by running the debug configuration.

## Step 1 -- tty enumerates at 921600 baud

Open the bridge's tty at 921600 baud with `picocom`, `minicom`, or similar:

```bash
picocom -b 921600 /dev/ttyUSB0    # Linux
picocom -b 921600 /dev/cu.usbmodemXXXX   # macOS
```

Expected behaviour: the port opens without error. Raw bytes you see will look
like garbage because they are **framed protobuf / log messages**, not ASCII.
That is the correct behaviour.

## Step 2 -- gateway decodes firmware log frames

With the firmware running, start the gateway binary pointing at the bridge
device:

```bash
cd star-gateway
go build -o bin/star-gateway ./cmd/star-gateway
# (environment variable names may vary; check cmd/star-gateway/main.go)
CDC_DEVICE=/dev/ttyUSB0 CDC_BAUD=921600 ./bin/star-gateway 2>&1 | tee gateway.log
```

Expected in `gateway.log`:

```
[RX72N] [INFO ] [BOOT ] RX72N boot complete
[RX72N] [INFO ] [COMM ] UART transport initialized (primary link to Pi5 gateway)
...
```

Every line prefixed `[RX72N]` came in as a `k_frame_type_log_message` (TYPE
`0x20`) frame and was routed to the gateway's log stream by the dispatcher in
`star-gateway/internal/manager/manager.go`.

## Step 3 -- PING / PONG heartbeat

The gateway's heartbeat module sends a PING every second when the link is
idle; the firmware must respond with PONG within 200ms. Watch for the
presence of heartbeat log lines (on both sides) over a minute. Missing PONG
within 200ms of PING indicates the UART framing path isn't round-tripping.

## Step 4 -- COMMAND / RESPONSE round-trip

Issue a motor velocity command via the gateway's gRPC or WS endpoint (exact
client depends on the tooling you use). Expected:

- The gateway encodes a protobuf `VelocityCommand`, wraps it as
  `FrameTypeCommand` (0x10), and writes 921600 baud bytes to
  `/dev/ttyUSB0`.
- The firmware decodes, applies the command, emits telemetry as
  `FrameTypeResponse` (0x11).
- The gateway's log shows both directions.
- The four wheel motors should spin per the command.

## Step 5 -- deliberate CRC error

Disconnect and reconnect the USB-UART cable mid-stream (or inject noise). The
gateway should:

- Log a CRC mismatch at the decoder.
- Advance the stream using `rx_frame_decode_with_resync` / equivalent on the
  Go side.
- Recover within <1s once clean bytes flow again.

## Pass criteria

- [ ] `/dev/ttyUSB0` (or macOS equivalent) enumerates at 921600 baud.
- [ ] Boot logs from the RX72N show up on the gateway as `[RX72N]` lines.
- [ ] PING / PONG heartbeat stays green for at least 60 seconds.
- [ ] One complete COMMAND -> motor-motion -> RESPONSE round-trip.
- [ ] CRC-error injection recovers within one second of clean input.

## Notes

- USB0 firmware is gone: there is no `/dev/ttyACM*` on the host anymore.
- Panic output (from `rx_check.h` / `rx_stack_monitor.c`) still writes RAW
  ASCII to SCI9 via `uart_debug_puts()`. The gateway's frame decoder will
  discard those bytes because they don't start with the 0x55AA sync word; the
  panic message is only visible through the E2 Lite debugger's RTT console
  or a direct serial terminal tapping TX before the bridge chip. This is a
  deliberate tradeoff -- the panic path should not depend on the RTOS being
  healthy enough to drain the log ring.
