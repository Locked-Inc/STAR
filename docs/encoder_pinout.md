# RX72N Encoder Pin Map -- TE Verification Sheet

Quadrature encoder A/B inputs as currently wired in firmware
(`star-rx72n-firmware/src/inc/hardware_config.h`). Cross-check each row
against the PCB schematic and confirm the physical wheel each encoder
actually routes to.

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

| Idx | Wheel (intent) | Timer | Phase A pin | Phase A pkg | Phase B pin | Phase B pkg | Source enums |
|-----|----------------|-------|-------------|-------------|-------------|-------------|--------------|
| 0   | front-left     | MTU1  | P24 / MTCLKA | 33 | P25 / MTCLKB | 32 | `k_encoder_0_phase_{a,b}_{port,pin}` |
| 1   | front-right    | MTU2  | PA1 / MTCLKC | 96 | PC5 / MTCLKD | 62 | `k_encoder_1_phase_{a,b}_{port,pin}` |
| 2   | back-right     | TPU1  | PC2 / TCLKA  | 70 | PA3 / TCLKB  | 94 | `k_encoder_2_phase_{a,b}_{port,pin}` |
| 3   | back-left      | TPU2  | PC0 / TCLKC  | 75 | PB3 / TCLKD  | 82 | `k_encoder_3_phase_{a,b}_{port,pin}` |

Port-bit detail (raw values from `hardware_config.h`):

| Idx | Phase A port | Phase A bit | Phase B port | Phase B bit |
|-----|--------------|-------------|--------------|-------------|
| 0   | 2  (PORT2) | 4 (P24) | 2  (PORT2) | 5 (P25) |
| 1   | 10 (PORTA) | 1 (PA1) | 12 (PORTC) | 5 (PC5) |
| 2   | 12 (PORTC) | 2 (PC2) | 10 (PORTA) | 3 (PA3) |
| 3   | 12 (PORTC) | 0 (PC0) | 11 (PORTB) | 3 (PB3) |

## Things to verify with the TE

1. **Schematic match:** does each (port, pin) pair in the table line up
   with the encoder header on the schematic for the named wheel?
2. **A/B order:** confirm that the pin labelled "Phase A" in the table
   really is the encoder's A channel (the one that leads B for forward
   wheel rotation). If A and B are swapped on a given wheel, the
   measured velocity sign will be inverted -- which the closed loop
   would interpret as the wrong direction and run away.
3. **Indexing consistency:** for each row, does that encoder's pin pair
   physically reach the SAME wheel that motor index N drives?
   - Motor 0 = GPTW0 (P23/P17) -> wheel
   - Motor 1 = GPTW1 (P22/PC3) -> wheel
   - Motor 2 = GPTW2 (PE3/P86) -> wheel
   - Motor 3 = GPTW3 (PE7/PC6) -> wheel
4. **Header naming on the PCB:** the silk-screen labels (E0/E1/E2/E3 or
   FL/FR/BR/BL) should match the same physical wheel as the motor
   header.

## Note on hardware_config.h labels

The header still names encoders `(rear-left)` for index 2 and
`(rear-right)` for index 3 in its docstrings. Those labels were written
under the old motor-index assumption (which had M2=BL, M3=BR). The
motor side has been fixed; the encoder docstrings haven't yet been
re-labelled. Pin assignments are unchanged -- only the human-readable
"this is the rear-left encoder" comments are stale.
