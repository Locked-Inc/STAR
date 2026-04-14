# BeagleBone Blue Firmware Architecture

## Overview

`star-beaglebone-blue` is a Linux userspace C application that serves as an
alternative motor controller to the RX72N and STM32 targets. It runs on
Debian Linux on the BeagleBone Blue (Octavo OSD3358 / AM335x Cortex-A8 @ 1 GHz).

Hardware features used:

- 4x bidirectional DC motor channels (DRV8838 H-bridges, via librobotcontrol)
- 4x quadrature encoder inputs (eQEP hardware, via librobotcontrol)
- 9-axis IMU: MPU-9250 accelerometer + gyroscope + AK8963 magnetometer
- USB device port: connects to RPi5 gateway as /dev/ttyGS0 (USB CDC ACM)

---

## System Communication

```
RPi5 Gateway (Go)
    |
    | USB CDC ACM (/dev/ttyGS0 on BBB, /dev/ttyACM0 on RPi5)
    |
BeagleBone Blue (star-beaglebone-blue)
    |-- comm_task         (100 Hz) -- frame parse + protobuf decode
    |-- motor_control_task (100 Hz) -- rc_motor_set + rc_encoder_read
    |-- imu_task           (100 Hz) -- MPU-9250 read
    |-- telemetry_task     (10 Hz)  -- protobuf encode + send
    |-- watchdog_task      (10 Hz)  -- 500 ms comm timeout -> estop
```

Wire protocol is identical to the RX72N and STM32:

```
| SYNC (2B) | SEQ (2B) | LEN (2B) | TYPE (1B) | FLAGS (1B) | PAYLOAD (N B) | CRC32 (4B) |
```

SYNC = 0x55AA (little-endian). CRC-32 covers all bytes from SYNC through end
of PAYLOAD.

---

## Task Architecture

### pthreads vs RTOS

Unlike the RX72N (ThreadX) and STM32 (FreeRTOS), the BBB target uses POSIX
pthreads with `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)` for
deterministic loop timing. The Linux scheduler is not hard real-time; for
applications requiring sub-millisecond motor control jitter, use the PRU
coprocessors (see PRU Roadmap below).

### Thread summary

| Thread            | Rate   | Reads from shared_data      | Writes to shared_data    |
|-------------------|--------|-----------------------------|--------------------------|
| comm_task         | 100 Hz | -                           | motor_cmd, last_cmd_us   |
| motor_control_task| 100 Hz | motor_cmd, estop            | encoder                  |
| imu_task          | 100 Hz | -                           | imu                      |
| telemetry_task    |  10 Hz | encoder, imu                | -                        |
| watchdog_task     |  10 Hz | last_cmd_us                 | estop                    |

### Shared data

`bb_shared_data_t` is a single struct protected by one `pthread_mutex_t`.
All tasks copy in or out under the mutex and release it immediately (O(memcpy)
hold time). No task holds the mutex while sleeping or doing I/O.

---

## Implementation Phases

### Phase 1 (current): Process scaffold
- [x] pthread creation and join in main()
- [x] 100 Hz / 10 Hz loop skeletons with clock_nanosleep
- [x] Shared data layer with mutex protection
- [x] USB CDC transport (bb_usb_cdc) via termios
- [x] Hardware init/deinit via librobotcontrol

### Phase 2: Wire framing and motor hardware
- [ ] Wire frame sync (SYNC = 0x55AA) and CRC-32 validation in comm_task
- [ ] rc_motor_set() duty cycles from motor_cmd in motor_control_task
- [ ] rc_encoder_read() and shared_data encoder update
- [ ] rc_mpu_initialize_dmp() and MPU-9250 reads in imu_task

### Phase 3: Protobuf integration
- [ ] nanopb decode MotorControlRequest in comm_task
- [ ] nanopb encode TelemetryResponse in telemetry_task
- [ ] bb_usb_cdc_send() of telemetry frames

### Phase 4: Robustness
- [ ] USB disconnect fast-path estop (bb_usb_cdc_is_connected check)
- [ ] rc_get_state() == EXITING check in all task loops for clean shutdown

### Phase 5 (optional): PRU real-time motor control
- [ ] PRU0 firmware: hardware PWM generation at precise frequency
- [ ] PRU1 firmware: encoder counting without scheduler jitter
- [ ] Replaces rc_motor_set / rc_encoder_read with PRU RPMSG interface

---

## Build

### Cross-compile from x86 host

```bash
# Install toolchain
sudo apt install gcc-arm-linux-gnueabihf cmake ninja-build

# Configure and build
cmake --preset cross-debug
cmake --build build-cross-debug -j$(nproc)
```

### Native build on BeagleBone Blue

```bash
# On the BBB
sudo apt install cmake ninja-build librobotcontrol-dev
cmake --preset native-debug
cmake --build build-native-debug -j$(nproc)
```

### Deploy to BBB

```bash
scp build-cross-debug/star-beaglebone-blue debian@beaglebone.local:~/
ssh debian@beaglebone.local sudo ./star-beaglebone-blue
```

---

## librobotcontrol API Reference

Key functions used (all from `<robotcontrol.h>`):

| Function                  | Purpose                                 |
|---------------------------|-----------------------------------------|
| rc_initialize()           | Board init, power management setup     |
| rc_kill_existing_process()| Ensure single firmware instance        |
| rc_set_state(RUNNING)     | Set process state machine              |
| rc_motor_init()           | Enable DRV8838 H-bridges               |
| rc_motor_set(ch, duty)    | Set motor duty cycle [-1.0, 1.0]       |
| rc_motor_set(0, duty)     | Set all motors (ch 0 = all channels)   |
| rc_motor_cleanup()        | Disable all motors and release         |
| rc_encoder_eqep_init()    | Enable eQEP quadrature decoder         |
| rc_encoder_read(ch)       | Read encoder tick count (int)          |
| rc_encoder_eqep_cleanup() | Release encoder hardware               |
| rc_mpu_initialize_dmp()   | Start MPU-9250 with DMP at N Hz        |
| rc_cleanup()              | Release all librobotcontrol resources  |
