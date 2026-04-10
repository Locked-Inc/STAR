# BeagleBone Blue Motor Controller Firmware

Userspace C firmware for the BeagleBone Blue (OSD3358/AM335x), serving as the
STAR robot's primary motor controller. Communicates with the Pi5 gateway over
USB CDC using the same nanopb+CRC-32 frame protocol as the RX72N fallback.

## Hardware

| Component | Spec |
|-----------|------|
| **Motors** | 4x DFRobot FIT0520 (6V brushed DC, 210 RPM, 3.2A stall, 34.02:1 gear ratio) |
| **Motor drivers** | 4x TI DRV8838 H-bridge (onboard, 0-11V VM, 1.7A continuous) |
| **Encoders** | Hall effect quadrature, 341 PPR (11,599 ticks/wheel rev with gear ratio) |
| **IMU** | InvenSense MPU-9250 (accel + gyro + mag, polled at 100 Hz) |
| **Communication** | USB CDC ACM (BBB /dev/ttyGS0 <-> Pi5 /dev/ttyACM0) |

## Power

Motors are powered directly from the battery rail (VBAT) through the DRV8838.
There is **no voltage regulator** between battery and motors.

| Source | Voltage | Powers Motors? |
|--------|---------|---------------|
| USB (micro-USB) | 5V | **No** -- SoC only |
| 2S LiPo (JST connector) | 6.0-8.4V | **Yes** |
| Barrel jack | 9-18V | **Yes** |

### Battery Voltage Duty Clamp

The firmware reads battery voltage via `rc_adc_batt()` (ADC channel 6, 11:1
resistor divider on the BBB Blue PCB) at 10 Hz and clamps motor duty cycles
to protect 6V motors from overvoltage:

```
max_duty = 0.95 * (6.0V / V_battery)
```

| Battery State | V_batt | Max Duty | Effective Motor V |
|---------------|--------|----------|-------------------|
| Full charge | 8.4V | 67.9% | 5.7V |
| Nominal | 7.4V | 77.0% | 5.7V |
| Discharged (BMS cutoff) | 6.0V | 95.0% | 5.7V |
| USB / no battery | 0.0V | 65.0% (fallback) | N/A |

Constants in `src/inc/hardware_config.h`:
- `k_bb_motor_rated_v = 6.0` -- motor rated voltage
- `k_bb_duty_derating = 0.95` -- 5% safety margin for ADC tolerance
- `k_bb_duty_fallback_max = 0.65` -- used when battery voltage unknown (USB power).
  Assumes worst-case 8.8V freshly charged 2S: `0.95 * 6.0 / 8.8 = 0.648`

The ADC reads the **total pack voltage** (both cells combined), not individual
cells. Per-cell monitoring and balancing is handled by the external BMS.

### Recommended Battery: 2S3P (6x 18650)

- **Cells**: EVE INR 18650/35V 3.5Ah (10A continuous, ~42 mohm)
- **Configuration**: 2S3P = 7.2V nominal, 10.5Ah, 30A continuous
- **BMS**: 2S 20A (handles balance, over-discharge, over-charge)
- **Charger**: TP5100 WH-370 module (CC/CV to 8.4V, 2A) -- charges through BMS
- **Runtime**: ~2.3 hours at normal driving (4.5A draw)
- **Charge time**: ~5.5 hours from empty at 2A

The BBB Blue's onboard charger (BQ24250) is **1S only** -- it cannot charge a 2S
pack. Use the TP5100 or any external 2S balance charger.

## Build

### Native build on BBB (Trixie has GCC 14)

```bash
ssh debian@192.168.7.2   # pw: StarBBB2026!
cd ~/star-beaglebone-blue
cmake --preset native-debug
cmake --build build-native-debug -j$(nproc)
```

### Cross-compile on Pi5

```bash
cd star-beaglebone-blue
cmake --preset cross-debug-pi5
cmake --build build-cross-debug -j$(nproc)
scp build-cross-debug/star-beaglebone-blue debian@192.168.7.2:~/
```

### Standalone motor test (no Pi5 needed)

```bash
# Build on Pi5:
arm-linux-gnueabihf-gcc -O2 -isystem /opt/bbb-sysroot/usr/include \
  -L/opt/bbb-sysroot/usr/lib/arm-linux-gnueabihf \
  -o motor-test scripts/motor-test.c -lrobotcontrol -lm -lpthread

# Deploy and run:
scp motor-test debian@192.168.7.2:~/
ssh debian@192.168.7.2 "sudo ./motor-test"              # full test
ssh debian@192.168.7.2 "sudo ./motor-test --no-motors"  # encoders + IMU only
ssh debian@192.168.7.2 "sudo ./motor-test --motor 1"    # single motor
```

## Run

```bash
ssh debian@192.168.7.2
sudo ./star-beaglebone-blue
# BBB opens /dev/ttyGS0, Pi5 sees /dev/ttyACM0
```

Or use `./start.sh` from the STAR repo root -- it auto-detects the BBB and
starts everything (firmware, gateway, ROS2, Foxglove).

## Architecture

See [docs/architecture.md](docs/architecture.md) for task thread design.

5 pthreads at 10-100 Hz:
- **comm_task** (100 Hz) -- USB CDC frame parse + protobuf decode
- **motor_control_task** (100 Hz) -- duty clamp + rc_motor_set + encoder read
- **imu_task** (100 Hz) -- MPU-9250 polling (accel + gyro)
- **telemetry_task** (10 Hz) -- protobuf encode + USB CDC send
- **watchdog_task** (10 Hz) -- 500ms comm timeout -> e-stop

## librobotcontrol Patches (5.10-ti kernel)

Stock librobotcontrol does not work on the 5.10-ti kernel. See
[docs/SETUP_AND_TROUBLESHOOTING.md](docs/SETUP_AND_TROUBLESHOOTING.md) for
the three required patches (PWM paths, encoder counter subsystem, I2C).
