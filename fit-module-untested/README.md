# fit-module-untested

A self-contained sandbox where the RX72N is brought up using **Renesas FIT
modules** (`r_bsp`, `r_sci_rx`, `r_riic_rx`, `r_gpt_rx`) instead of the
custom HAL in `star-rx72n-firmware/`. Speaks ROS2 via a tiny Python bridge
on the Pi5 — no micro-ROS, no SPI, no nanopb. Just `/dev/ttyACM0` carrying
a CRC-8 framed binary protocol.

> The folder name is a contract. **Nothing here ships to the robot until it
> is promoted out of this directory.** The driver glue is intentionally a
> skeleton that needs validation against the actual FIT vendor headers and
> against the bench.

## Why this exists

`star-rx72n-firmware/` is a custom HAL, register-poked, that we know works.
This sandbox evaluates the alternative: import Renesas' canonical FIT
drivers and let them own clock setup, MSTP releases, SCI/RIIC/GPTW init.

Reasons it could be worth it:
- New peripherals (Ethernet, USB host, SDHI) where writing custom HAL is
  weeks of work.
- Cross-check our HAL's clock tree, MSTP table, and PFS unlock sequence
  against Renesas' canonical sources.

Reasons it might not survive: each FIT API has slightly different field
names and call shapes per version, and the GPTW module in particular has
been less stable than `r_sci`/`r_riic`. v0 here is a starting point, not a
finished port.

## What's in the box

```
fit-module-untested/
+-- README.md                     <- you are here
+-- CMakeLists.txt                <- standalone, no add_subdirectory(../...)
+-- cmake/toolchain-rx72n.cmake   <- copy of star-rx72n-firmware/cmake/...
+-- scripts/
|   +-- fetch-fit-modules.sh      <- clones renesas/rx-driver-package + unzip
|   +-- flash.sh                  <- box64 + rfp-cli wrapper (DO NOT RUN
|                                    without explicit approval)
+-- vendor/                       <- gitignored, populated by fetch script
|   +-- FITModules/
|       +-- r_bsp/
|       +-- r_sci_rx/
|       +-- r_riic_rx/
|       +-- r_gpt_rx/
|       +-- r_byteq/
+-- config/                       <- our overrides for the FIT _config.h files
|   +-- r_bsp_config.h
|   +-- r_sci_rx_config.h         (SCI9, 921600 8N1, buffered TX/RX)
|   +-- r_riic_rx_config.h        (RIIC1, 400 kHz)
|   +-- r_gpt_rx_config.h         (GPTW0..3, 20 kHz PWM)
|   +-- r_byteq_config.h
|   +-- r_bsp_pin_config.h        (intentionally empty -- pin-mux in hw_init.c)
+-- src/
|   +-- main.c            <- super-loop: 100 Hz IMU TX, RX MOTOR_CMD parser
|   +-- hw_init.[ch]      <- pin-mux + GPIO setup; cites uart_test/imu_test
|   +-- bno055.[ch]       <- IMU mode, accel + gyro at 100 Hz
|   +-- motors.[ch]       <- DRV8263H sign-magnitude PWM via GPTW
|   +-- serial_proto.[ch] <- frame encode + RX state machine
|   +-- crc8.[ch]         <- Dallas/Maxim CRC-8 lookup
|   +-- linker_script.ld  <- copy of star-rx72n-firmware/src/linker_script.ld
+-- pi_node/                      <- ROS2 ament_python package
    +-- package.xml
    +-- setup.py / setup.cfg
    +-- star_serial_bridge/star_serial_bridge.py
    +-- launch/star_bridge.launch.py
    +-- config/ekf.yaml
    +-- resource/star_serial_bridge
```

## Bring-up sequence

> **Stop here and read.** None of these steps will be run automatically.
> Each one needs an explicit approval before it executes anything that
> touches the network or the hardware.

### 1. Fetch FIT modules (network)

```sh
cd /workspaces/STAR/fit-module-untested
./scripts/fetch-fit-modules.sh
```

That clones `https://github.com/renesas/rx-driver-package` into a temp
directory, runs its top-level `Makefile` (which downloads the latest FIT
zips into a `FITModules/` directory in the clone), then unzips just the
modules listed in `MODULES=(...)` at the top of the script into
`vendor/FITModules/`. `vendor/` is gitignored so we never check vendor
source into the repo.

To pin a specific upstream tag for reproducibility, set `RX_DRIVER_TAG`
near the top of `scripts/fetch-fit-modules.sh`.

### 2. Build

```sh
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rx72n.cmake
cmake --build build -j
```

Output: `build/fit-module-untested.elf` and `build/fit-module-untested.mot`.

> **Expect compile failures.** Several call sites in `bno055.c`, `motors.c`,
> and `main.c` reference FIT API symbols (`R_SCI_Open`, `R_RIIC_Open`,
> `R_GPT_PWM_Set`, `riic_info_t`, `gpt_pwm_t`, `SCI_EVT_RX_CHAR`, etc.) by
> their published names. Field names and exact spellings have shifted between
> FIT versions. After the fetch script runs, open the corresponding `r_*_if.h`
> in `vendor/FITModules/` and reconcile any mismatches. Each affected file
> includes a header comment listing the symbols to verify.

### 3. Flash (DO NOT auto-run)

```sh
./scripts/flash.sh build/fit-module-untested.mot
```

Wrapped in `box64` because `rfp-cli` on this Pi5 is x86_64 (see memory
`project_rfp_cli_box64.md`). E2 Lite must be attached.

### 4. Pi-side ROS2 bridge

```sh
mkdir -p ~/ros2_ws/src
ln -s /workspaces/STAR/fit-module-untested/pi_node ~/ros2_ws/src/star_serial_bridge
cd ~/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select star_serial_bridge
source install/setup.bash
ros2 launch star_serial_bridge star_bridge.launch.py
```

### 5. Verification

In a second shell:

```sh
source ~/ros2_ws/install/setup.bash
ros2 topic hz /imu/data         # expect ~100 Hz
ros2 topic echo /imu/data --once
ros2 topic pub --once /cmd_vel geometry_msgs/Twist '{linear: {x: 0.05}}'
```

Watchdog test: after publishing `/cmd_vel`, kill the publisher (`Ctrl+C`).
The firmware's 500 ms watchdog should zero duties. Then bring up the bridge
launcher cleanly with `Ctrl+C` -- on shutdown the bridge sends an `ESTOP`
frame and the firmware drops `DRVOFF` regardless of motor command state.

## Wire protocol summary

| Direction        | Type | Payload                                  | Period |
| ---------------- | ---- | ---------------------------------------- | ------ |
| RX72N -> Pi      | 0x01 | `float[6]` accel/gyro + `uint32` ts_ms   | 100 Hz |
| Pi -> RX72N      | 0x02 | `int16[4]` duty (-10000..+10000)         | on /cmd_vel |
| Pi -> RX72N      | 0x03 | (none)                                   | on shutdown |
| RX72N -> Pi      | 0x04 | `uint8 imu_ok, uint8 armed, uint32 up`   | 1 Hz   |

Frame: `0xAA 0x55 TYPE LEN <payload> CRC8`. CRC over `TYPE..last payload byte`.

The Python bridge mirrors the C state machine in
`fit-module-untested/src/serial_proto.c`. If you change the wire on either
side, change it on both.

## Out of scope for v0

| Feature                | Lives where                        |
| ---------------------- | ---------------------------------- |
| Encoder reading        | v1: extend protocol with ODOM type |
| ADC current sensing    | v1                                 |
| Closed-loop wheel PID  | v1, after encoders                 |
| ThreadX / RTOS         | only if super-loop starves         |
| EKF + slam_toolbox     | sketched in launch + ekf.yaml      |

## License

Source under this directory is MIT (matches the rest of STAR). Renesas FIT
modules under `vendor/FITModules/` are governed by Renesas' permissive
license; we don't redistribute them (gitignored), so the obligation is just
"abide by the upstream terms when you fetch" -- read the headers in any zip
the script unpacks.

## Known sharp edges

- `r_bsp_config.h` clock tree assumes our STAR PCB matches the Envision Kit
  EXTAL (12 MHz). Diff against `star-rx72n-firmware/libs/rx_hal/inc/rx72n_clock.h`
  before first power-on -- a wrong PCLKB silently miscomputes SCI9 BRR.
- `r_gpt_rx` API has historically been the wobbliest of the FIT modules.
  Symbols in `motors.c` (`gpt_pwm_t`, `R_GPT_PWM_Set`, `GPT_CH0`) may need
  renaming to match what your fetched version of `r_gpt_rx_if.h` exports.
- `r_bsp` ships its own startup + reset vectors. Our `linker_script.ld`
  expects entry symbol `_PowerON_Reset` which `r_bsp` provides. Don't
  resurrect a separate `reset_program.S`.
- DRV8263H power-up: nSLEEP high -> wait tWAKE -> DRVOFF low -> PWM. Skipping
  the wait gives `OUT1`/`OUT2` stuck at Vm/2 (memory:
  `reference_drv8263h_quiescent_signature.md`).
- USB-CDC enumerates as `/dev/ttyACM0` (Cypress CY7C65213, VID/PID 04b4:0003),
  **not** `/dev/ttyUSB0`. Do not blindly trust web tutorials that say the
  latter.
