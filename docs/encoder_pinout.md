# RX72N Encoder Pin Map

Quadrature encoder A/B inputs as wired in firmware
(`star-rx72n-firmware/src/inc/hardware_config.h`). Verified against the
production PCB schematic on 2026-04-25.

**Encoder source:** 6 V brushed DC gearmotor with integrated 341 PPR
quadrature Hall encoder. Firmware decodes 4x (1364 counts/rev).

**Encoder index <-> motor index correspondence:**
Encoder index N is consumed as the velocity feedback for motor index N
(see `motor_control_task.c::internal_read_encoder_velocity`). For the
closed loop to stabilize, encoder N must physically connect to the
same wheel that motor N drives.

After the recent motor wiring fix, motor indices map to wheels as:
- index 0 = front-left
- index 1 = front-right
- index 2 = back-right
- index 3 = back-left

So encoder indices should match. If a row below labels a different
wheel, flag it for fix.

## Encoder pin table

| Idx | Wheel       | Timer | Phase A pin | Phase A pkg | Phase B pin | Phase B pkg | Source enums |
|-----|-------------|-------|-------------|-------------|-------------|-------------|--------------|
| 0   | front-left  | MTU1  | P24 / MTCLKA | 33 | P25 / MTCLKB | 32 | `k_encoder_0_phase_{a,b}_{port,pin}` |
| 1   | front-right | MTU2  | PA1 / MTCLKC | 96 | PC5 / MTCLKD | 62 | `k_encoder_1_phase_{a,b}_{port,pin}` |
| 2   | back-right  | TPU1  | PC2 / TCLKA  | 70 | PA3 / TCLKB  | 94 | `k_encoder_2_phase_{a,b}_{port,pin}` |
| 3   | back-left   | TPU2  | PC0 / TCLKC  | 75 | PB3 / TCLKD  | 82 | `k_encoder_3_phase_{a,b}_{port,pin}` |

Port-bit detail (raw values from `hardware_config.h`):

| Idx | Phase A port | Phase A bit | Phase B port | Phase B bit |
|-----|--------------|-------------|--------------|-------------|
| 0   | 2  (PORT2) | 4 (P24) | 2  (PORT2) | 5 (P25) |
| 1   | 10 (PORTA) | 1 (PA1) | 12 (PORTC) | 5 (PC5) |
| 2   | 12 (PORTC) | 2 (PC2) | 10 (PORTA) | 3 (PA3) |
| 3   | 12 (PORTC) | 0 (PC0) | 11 (PORTB) | 3 (PB3) |

## Motor-encoder index correspondence

For the closed loop to stabilize, encoder index N must physically
connect to the SAME wheel that motor index N drives. The current wiring
(verified):

| Motor index | Wheel       | Motor PWM pins (GPTW)  |
|-------------|-------------|------------------------|
| 0           | front-left  | GPTW0 (P23 / P17)      |
| 1           | front-right | GPTW1 (P22 / PC3)      |
| 2           | back-right  | GPTW2 (PE3 / P86)      |
| 3           | back-left   | GPTW3 (PE7 / PC6)      |

`hardware_config.h` docstrings (`back-right` for index 2, `back-left`
for index 3) match this table.
