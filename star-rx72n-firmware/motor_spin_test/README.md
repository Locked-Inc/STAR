# motor_spin_test -- 4-motor open-loop spin test (production STAR PCB)

Bare-metal bench test that drives all four DRV8263H H-bridges from the RX72N
GPTW peripheral with a -100..+100..-100 percent duty sweep. Reuses the
production motor-control libraries (`libs/rx_motor`, `libs/rx_hal/rx_gptw`,
`libs/rx_hal/rx_mpc`) and the production crystal+PLL clock setup, but skips
ThreadX, USB, SPI, IMU, sonar, ADC, and the closed-loop PID task.

Use this to confirm motor wiring, H-bridge enable, and PWM signal integrity
before bringing up the full firmware.

## Hardware preconditions

- Real STAR PCB (not Tom's breakout) with 24 MHz crystal populated.
- Motor bus voltage applied to the barrel/battery input. **USB power alone
  cannot spin motors** -- the DRV8263H VM rail must be live.
- E2 Lite debug probe (FINE) connected for flash + run.
- Optional: scope on PA4 (heartbeat) plus any motor IN1/IN2 pin to confirm
  PWM activity.

## Pin map (mirrors `src/inc/hardware_config.h`)

| Motor | GPTW | IN2 (A) | IN1 (B) | DRVOFF | nSLEEP |
|-------|------|---------|---------|--------|--------|
| 0     | 0    | P23 (34)| P17 (38)| P61 (115) | P60 (117) |
| 1     | 1    | P22 (35)| PC3 (67)| P63 (113) | P62 (114) |
| 2     | 2    | PE3 (108)| P86 (41)| PE0 (111) | P64 (112) |
| 3     | 3    | PE7 (101)| PC6 (61)| PE2 (109) | PE1 (110) |

Heartbeat: PA4 toggles every duty step (~40 ms). Solid HIGH after init means
`rx_motor_init()` failed -- check the wiring table.

## What it does

1. `clock_init()` -- 24 MHz crystal -> PLL 240 MHz -> ICLK=240, PCKA=120 MHz.
   Self-contained equivalent of `src/rx_clock_power_init.c` (no logging,
   no asserts, no PPLL/USB).
2. `motor_drv_gpio_init()` -- the same safe DRV8263H power-up sequence
   `internal_gpio_init_motor_driver_ctrl()` uses:
   DRVOFF HIGH, then nSLEEP HIGH, then ~10 ms tWAKE, then DRVOFF LOW.
3. `motor_pwm_init()` -- mux all 8 GPTW pins via `rx_mpc_set_gptw()`
   (handles the port-1/2 vs port-C/E PSEL split internally), then
   `rx_motor_init()` for each channel at 20 kHz / 1 us deadtime.
4. Forever loop: `rx_motor_set_duty()` for all 4 motors stepping by 5%
   from -100 -> +100 -> -100. Heartbeat toggles each step.

## One-time files to drop in

The bench-test boot files are identical to `gpio_test/`. From the firmware
root run:

```bash
cp gpio_test/startup.S    motor_spin_test/
cp gpio_test/vectors.S    motor_spin_test/
cp gpio_test/linker.ld    motor_spin_test/
```

(They are MCU boot boilerplate -- no test-specific content.)

## Build and flash

```bash
cd star-rx72n-firmware/motor_spin_test
make clean && make
make flash    # E2 Lite via rfp-cli
```

`make` should report a small ELF (a few KB text + the rx_motor/rx_gptw/rx_mpc
objects). If linking fails for an `internal_log_*` or `uart_debug_*` symbol,
add the missing prototype to `stubs.c` -- it is intentionally a no-op shim.

## Expected behaviour

- PA4 toggles at ~12 Hz throughout the sweep.
- All four motors ramp slowly forward, brake, reverse, and repeat.
- Each motor channel shows complementary PWM on IN1/IN2 (one is duty,
  the other is held LOW) at 20 kHz on the scope.
- DRVOFF stays LOW (driver enabled) for the duration; nSLEEP stays HIGH.

## Safety notes

- The duty sweep crosses 0% via active brake (both outputs LOW). If a
  wheel is mechanically loaded, expect a momentary stop at each direction
  reversal.
- `rx_motor_init()` will fail if the GPTW pin mux returned an error
  (typically a pin that is not GPTW-capable). On failure the test parks
  PA4 solid HIGH and spins -- no PWM is generated.
- If you smell magic smoke, kill the bus voltage. The DRVOFF=LOW step
  happens before any non-zero duty, so a wiring fault on IN1/IN2 will
  show up as PWM into a wrong load before the first real motion.

## Removing the test

Delete the directory; the test does not modify any production source.
