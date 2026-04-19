# pwm_test -- minimal 20 kHz PWM signal generator

Brings up GPTW0 channel 0 on **P23 (GTIOC0A)** and **P17 (GTIOC0B)** --
the two motor-0 IN1/IN2 pins -- so you can scope the PWM signals on the
Analog Discovery 2 **without any motor or H-bridge connected**.

## Wiring (Tom's PCB header)

| AD2 probe            | Tom header pin | RX72N pin / function |
|----------------------|----------------|----------------------|
| **Scope Ch1+ (orange)** | **pin 38**   | P17 / GTIOC0B        |
| **Scope Ch2+ (blue)**   | **pin 34**   | P23 / GTIOC0A        |
| **Scope Ch1- / Ch2- (grounds)** | any GND pad | GND     |

Optional digital logic-analyzer capture:

| AD2 DIO | Tom header pin | RX72N pin |
|---------|----------------|-----------|
| DIO 0   | pin 38         | P17       |
| DIO 1   | pin 34         | P23       |
| DIO GND | any GND pad    | GND       |

AD2 scope settings for a clean view:
- Time base: **10 us/div** (one 20 kHz period is 50 us -> 5 divisions)
- Volt scale: **500 mV/div** (3.3 V logic fits in 7 divisions)
- Trigger: Ch1 rising edge, mid-scale

## Build and flash

```bash
cd star-rx72n-firmware/pwm_test
make clean && make
make flash      # uses rfp-cli via E2 Lite
```

## What you will see

1. **PA7 status LED** toggles on every duty-sweep step (proves firmware is alive).
2. **Ch1 (P17)** and **Ch2 (P23)** output a **20 kHz square wave**.
3. Duty cycle sweeps **0 -> 100% -> 0%** over ~4 s, then repeats.
4. Ch1 and Ch2 are **complementary** -- as Ch1's HIGH time grows,
   Ch2's HIGH time shrinks (sign-magnitude H-bridge style).

If you capture this with AD2's logic-analyzer DIO you'll see a clean
square wave on both lines at exactly 20 kHz with bit-perfect duty
cycle values.

## Clock path

```
HOCO 16 MHz -> PLL x12 = 192 MHz
              v
             PCKA = 96 MHz   (GPTW clock source)
              v
             GTPR = 4800 - 1 = 4799
              v
             PWM period = 96 MHz / 4800 = 20 kHz
```

## Extending to motors 1-3

Once you see clean PWM on Ch1/Ch2, the same pattern extends to:

| Motor | GTIOCxA (IN2) | GTIOCxB (IN1) | GPTW base   |
|-------|---------------|---------------|-------------|
| 0     | P23 / pin 34  | P17 / pin 38  | 0x000C2000 |
| 1     | P22           | PC3           | 0x000C2100 |
| 2     | PE3           | P86           | 0x000C2200 |
| 3     | PE7           | PC6           | 0x000C2300 |

The MSTPCRA.MSTPA7 bit gates all four GPTW channels -- clear it once
and all channels are available.
