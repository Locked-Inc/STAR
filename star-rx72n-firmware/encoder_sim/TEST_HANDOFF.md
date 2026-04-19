# Encoder Simulation + Sync PID Test Handoff (Windows)

You are testing a 4-motor wheel-synchronization pipeline for the STAR
robotics project. Everything you need is in this repo under the paths noted
below. Host is **Windows 10/11 with Docker Desktop + WSL2**. Builds run
inside a Linux devcontainer; flashing and AD2 playback run on the Windows
host.

Repo root in this doc: `%STAR%` (i.e. wherever you cloned the STAR repo).

---

## What this system does

1. Replay quadrature encoder waveforms (real scope captures or synthetic
   RPM profiles) through an **Analog Discovery 2 (AD2)** into the
   **TOM PCB** (a 144-pin RX72N dev board with exposed GPIO).
2. The RX72N firmware reads those waveforms as if they were real motor
   encoders, and runs a 4-motor velocity-sync PID.
3. The sync layer forces all 4 motors' effective setpoints to the
   *minimum* of measured velocities, so if one wheel slows, all wheels
   slow to match (keeps the robot tracking straight under a worn motor).
4. MATLAB simulations predict the firmware's duty-cycle output. Firmware
   and MATLAB should agree.

---

## Hardware checklist

Connect all of the following *before* starting the tests:

- [ ] **TOM PCB -- both USB cables plugged in**. One on the SCI/Debug
      USB-C (powers the Cypress USB-UART and the 3.3 V rail), one on
      the USB0 USB-C. Both must stay connected for a stable rail.
- [ ] **USB-to-UART dongle** plugged into the TOM PCB E2 / JTAG header
      (J13), 3.3 V logic. Wiring:
      - dongle TX -> J13 pin 12 (TDI, = RXD1 on the RX72N boot ROM)
      - dongle RX -> J13 pin 10 (TDO, = TXD1 on the RX72N boot ROM)
      - dongle GND -> J13 pin 2 (or 4/6/8/14, any GND)
- [ ] **Analog Discovery 2** (Digilent, SN **210321A36AAE**, the one
      labelled "B"), wired to the TOM PCB headers per the table below.
- [ ] **SW1 on TOM PCB**: SW1.1 = **ON**, SW1.2 = **OFF** (SCI boot
      mode).

### AD2 -> TOM PCB wiring

3.3 V logic on both sides, no level shifter needed.

**Important**: The TOM PCB does not break out Motor 0's encoder pins
(P24 / P25) to any header. Motor 0 stays unwired during this test --
we can only exercise 3 of 4 motors. The sync PID algorithm still
validates because minimum-tracking across 3 motors proves the
degradation behaviour.

| AD2 DIO | TOM PCB header pin | RX72N signal |
|---------|--------------------|--------------|
| DIO 0 | *(unused, leave floating)* | ~~Motor 0 Phase A~~ |
| DIO 1 | *(unused, leave floating)* | ~~Motor 0 Phase B~~ |
| DIO 2 | **J7 pin 13** -- PA1 | Motor 1 (FR) Phase A |
| DIO 3 | **J10 pin 3** -- PC5 | Motor 1 (FR) Phase B |
| DIO 4 | **J10 pin 11** -- PC2 | Motor 2 (BL) Phase A |
| DIO 5 | **J7 pin 15** -- PA3 | Motor 2 (BL) Phase B |
| DIO 6 | **J14 pin 2** -- PC0 | Motor 3 (BR) Phase A |
| DIO 7 | **J11 pin 12** -- PB3 | Motor 3 (BR) Phase B |
| AD2 GND | any GND pin on TOM PCB | ground |

---

## Prerequisites (Windows host)

### 1. Python 3.10+ and pydwf

```
py -3 -m pip install pydwf
```

### 2. Digilent WaveForms runtime (the AD2 driver)

Install from https://digilent.com/shop/software/digilent-waveforms/
(the installer is called `digilent.waveforms_vXXXX.exe`). This places
`dwf.dll` where pydwf can find it.

Sanity check:
```
py -3 -c "from pydwf import DwfLibrary; print(DwfLibrary().getVersion())"
```
Should print a version like `3.24.2`.

### 3. MATLAB with Control System Toolbox (any version >= R2023a)

Optional -- only needed for Tests 1 and 2. Skip if you don't have it.

### 4. Docker Desktop with WSL2 backend

Only needed if you're going to rebuild the firmware (not needed for
Tests 1-4). Start it before running `devcontainer up`.

### 5. Renesas Flash Programmer CLI (for flashing, not for Tests 1-4)

Download the RFP Windows installer from the Renesas website. It places
`rfp-cli.exe` in its install directory (typically
`C:\Program Files (x86)\Renesas Electronics\Programming Tools\Renesas
Flash Programmer V3.XX\rfp-cli.exe`).

---

## Finding your COM port for the UART dongle

PowerShell:
```
[System.IO.Ports.SerialPort]::GetPortNames()
```

Or Device Manager -> Ports (COM & LPT). Note which `COMx` is your
USB-to-UART dongle. Typical values: `COM3`, `COM4`, `COM5`.

---

## Test plan

### Test 1 -- MATLAB synchronization simulation (no hardware needed)

```
cd %STAR%\matlab
matlab -batch "sync_pid_sim"
```

**Expected**: a figure saved to `%STAR%\figures\sync_pid_sim.png`
showing all 4 motors at 210 RPM until t=5 s, then all 4 converging to
~105 RPM within 500 ms.

**Pass criterion**: console prints `max sync error = X.XX RPM [PASS]`
with `X.XX < 5`.

### Test 2 -- MATLAB robustness sweep (no hardware needed)

```
matlab -batch "sync_robustness_sweep"
```

**Expected**: `%STAR%\figures\sync_robustness_sweep.png` heatmap.

**Pass criterion**: `Fraction of sweep passing (< 5 RPM)` prints as
`100%`.

### Test 3 -- Synthetic AD2 playback

AD2 connected, no TOM PCB or scope needed beyond a voltmeter / logic
probe to verify output.

```
cd %STAR%\star-rx72n-firmware\encoder_sim
py -3 encoder_sim.py --no-loop
```

The script plays the 10-second synthetic `phase_b` profile (motor 2
degrades at t=5 s) through 8 DIO channels, using the `encoder_sim_phase_a.csv`
and `encoder_sim_phase_b.csv` already in this directory.

**Pass criterion**: probe DIO 2 with a scope or multimeter -- it should
toggle between 0 V and ~3.3 V at ~1200 Hz for the first 5 seconds, then
slow down.

### Test 4 -- Real-data AD2 playback (recommended)

```
py -3 encoder_sim.py ^
    --phase-a encoder_sim_phase_a_real.csv ^
    --phase-b encoder_sim_phase_b_real.csv ^
    --no-loop
```

The `*_real.csv` files were generated by running
`convert_scope_capture.py` against a 1.2 GB dual-Keysight-scope
capture of 4 real wheels (15 x 100 ms bursts, stitched). You do not
need to re-run the converter; the output CSVs are checked in.

**Pass criterion**: DIO 2..7 all toggle between 0 V and ~3.3 V for
~1.5 seconds. Actual encoder rate in the real data: ~1150 Hz on each
A / B channel.

### Test 5 -- Flash + firmware readback (advanced, skip for now)

This requires a firmware build that calls `rx_sync_update()` and
`rx_sync_get_setpoint()` in the 250 Hz control task, and a simple UART
printout of each motor's velocity. That integration has **not yet
been committed to main**. Skip Test 5 unless Brighton explicitly
provides you with an ELF or `.mot` file to flash.

If you do get a file to flash, the procedure is in the "Flashing
procedure" section below.

---

## Flashing procedure (for when you eventually need it)

Only needed for Test 5 or for re-flashing if the board gets into a
weird state. **Power-cycle the board between every flash** -- the
RX72N boot ROM only listens for a few seconds after reset.

### Step-by-step

1. On the TOM PCB, verify **SW1.1 = ON, SW1.2 = OFF**.
2. **Unplug both USB-C cables from the TOM PCB** (removes power).
3. **Plug both USB-C cables back in** (restores power; boot ROM opens
   its SCI listen window).
4. Find your UART dongle's COM port:
   ```
   [System.IO.Ports.SerialPort]::GetPortNames()
   ```
5. Run rfp-cli (replace `COM3` with your actual COM port and adjust
   the path if the installer put it somewhere different):
   ```
   "C:\Program Files (x86)\Renesas Electronics\Programming Tools\Renesas Flash Programmer V3.22\rfp-cli.exe" ^
       -device RX72x ^
       -port COM3 ^
       -if uart ^
       -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF ^
       -a ^
       -file <path-to-firmware.mot> ^
       -run
   ```

If you get `E3000105: The device is not responding`, it almost always
means one of:

- The board wasn't power-cycled after the previous flash (boot ROM
  window closed -> re-cycle power and try again, fast).
- Wrong COM port (check Device Manager).
- Dongle TX / RX swapped (dongle TX goes to TDI, dongle RX goes to
  TDO -- see the wiring table at the top of this doc).
- SW1.1 / SW1.2 in the wrong position.

### Building firmware (devcontainer flow)

Only needed if you're changing C code. For Tests 1-4 you don't need
to build anything. For Test 5, if you need to rebuild from source:

```
cd %STAR%
devcontainer up --workspace-folder .
devcontainer exec --workspace-folder . bash -lc ^
    "cd star-rx72n-firmware && ./build.sh"
```

The output lands at `star-rx72n-firmware\build\star-rx72n-firmware.elf`
which you then flash with rfp-cli (see above).

Why the split? macOS and Windows Docker Desktop cannot forward USB
devices into containers, so the flasher has to run on the host.
GNURX's cross-compile toolchain is Linux-only, so the compiler has to
run inside the container. Build in Linux, flash from Windows.

---

## What to report back to Brighton

For each test:
1. The command you ran.
2. The last ~30 lines of console output.
3. Pass / fail against the stated criteria.
4. Any PNG plots produced (attach them).

If a test fails, also include:
- Output of `[System.IO.Ports.SerialPort]::GetPortNames()` (PowerShell)
- Current SW1 / EMLE jumper positions on the TOM PCB.
- Which AD2 DIO pins are physically wired (double-check against the
  table above).

Do not modify any code without asking. Running the existing scripts
and `pip install`-ing any missing packages they ask for is fine.

---

## Directory map

Everything you need is under one of these three directories:

```
%STAR%\
+-- matlab\                              (Tests 1, 2)
|   +-- motor_params.m                   motor constants
|   +-- motor_model_1st_order.m          plant model G(s) = 3.665 / (0.075s + 1)
|   +-- pid_design_velocity.m            pidtune() PI design
|   +-- sync_pid_sim.m                   <- Test 1
|   +-- sync_robustness_sweep.m          <- Test 2
|   +-- firmware_match_sim.m             single-precision firmware mirror
+-- star-rx72n-firmware\
|   +-- encoder_sim\                     (Tests 3, 4)
|   |   +-- TEST_HANDOFF.md              this file
|   |   +-- gen_encoder_csv.py           synthetic CSV generator
|   |   +-- convert_scope_capture.py     scope-capture -> CSV converter (docs)
|   |   +-- encoder_sim.py               <- Tests 3, 4 (AD2 playback)
|   |   +-- encoder_sim_phase_a.csv      synthetic, phase_b profile
|   |   +-- encoder_sim_phase_b.csv
|   |   +-- encoder_sim_phase_a_real.csv <- real scope data (Test 4)
|   |   +-- encoder_sim_phase_b_real.csv
|   +-- libs\rx_sync\                    sync-layer firmware module (Test 5)
|       +-- inc\rx_sync.h
|       +-- src\rx_sync.c
+-- scripts\flash-rx72n.sh               flashing wrapper (macOS/Linux only,
                                         Windows users run rfp-cli directly --
                                         see "Flashing procedure" above)
```

---

## Ready-to-paste Claude context (if you need Claude's help)

> I'm testing a 4-motor wheel-synchronization pipeline for STAR. The
> simulation + PID design + AD2 playback scaffolding is in
> `star-rx72n-firmware\encoder_sim` and `matlab\`. Hardware: AD2 (SN
> 210321A36AAE) wired to a Renesas RX72N dev board ("TOM PCB") via
> DIO channels 2..7 (DIO 0/1 unused -- Motor 0 pins not broken out).
> I'm on Windows 10/11. See `star-rx72n-firmware\encoder_sim\TEST_HANDOFF.md`
> in the STAR repo for the full context, wiring table, test plan, and
> flashing procedure.
