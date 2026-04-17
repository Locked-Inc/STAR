# STAR Bill of Materials

Verified against the schematic and the implemented sensor wiring. Unit
costs are 2026 retail prices at common U.S. suppliers; bulk pricing for
the team's actual purchases is in the course accounting spreadsheet.

---

## Compute

| Part | Vendor | Qty | Unit cost | Datasheet |
|---|---|---|---|---|
| Raspberry Pi 5 (8 GB) | Canakit / Adafruit | 1 | $80 | [raspberrypi.com/documentation/computers/raspberry-pi-5.html](https://www.raspberrypi.com/documentation/computers/raspberry-pi-5.html) |
| Raspberry Pi 5 active cooler | official | 1 | $5 | |
| microSD card 64 GB (U3, A2) | SanDisk | 1 | $15 | |
| Renesas RX72N (R5F572NNHxFB, 144-pin LFQFP) | Digikey | 1 | $22 | [renesas.com/us/en/products/microcontrollers-microprocessors/rx-32-bit-performance-efficiency-mcus/rx72n](https://www.renesas.com/us/en/products/microcontrollers-microprocessors/rx-32-bit-performance-efficiency-mcus/rx72n) |
| Custom PCB (4-layer, RX72N breakout, motor drivers, power, IMU) | JLCPCB | 1 | $60 (prototype run) | see `schematic/` |

## Sensors

| Part | Vendor | Qty | Unit cost | Datasheet |
|---|---|---|---|---|
| SLAMTEC RPLiDAR C1 | SLAMTEC | 1 | $110 | [slamtec.com/en/C1](https://www.slamtec.com/en/C1) |
| Waveshare IMX219-83 Stereo Camera (dual Sony IMX219 8 MP, 60 mm baseline, onboard ICM20948 IMU) | Waveshare / Amazon | 1 | $95 | [waveshare.com/wiki/IMX219-83_Stereo_Camera](https://www.waveshare.com/wiki/IMX219-83_Stereo_Camera) |
| Bosch BNO055 9-DoF absolute orientation IMU | Adafruit 2472 / Sparkfun | 1 | $35 | [bosch-sensortec.com/products/smart-sensors/bno055](https://www.bosch-sensortec.com/products/smart-sensors/bno055) |
| HC-SR04 ultrasonic rangefinder | Adafruit | 4 | $4 each ($16) | [mouser.com/datasheet/2/813/HCSR04-1022824.pdf](https://www.mouser.com/datasheet/2/813/HCSR04-1022824.pdf) |
| Maxim DS18B20+ 1-Wire temperature sensor | Adafruit 374 | 1 | $4 | [maximintegrated.com/en/products/sensors/DS18B20.html](https://www.analog.com/en/products/ds18b20.html) |
| Bosch BMP280 pressure + temp sensor | Adafruit 2651 | 1 | $10 | [bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280](https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280) |

## Motor drivetrain

| Part | Vendor | Qty | Unit cost | Datasheet |
|---|---|---|---|---|
| DFRobot FIT0520 6 V 210 RPM gearmotor w/ 341 PPR Hall encoder | DFRobot | 4 | $22 ($88) | [wiki.dfrobot.com/Micro_DC_Geared_Motor_w_Encoder](https://wiki.dfrobot.com/Micro_DC_Geared_Motor_w_Encoder_SKU_FIT0520) |
| TI DRV8263H H-bridge motor driver | Digikey | 4 | $4 ($16) | [ti.com/product/DRV8263](https://www.ti.com/product/DRV8263) |
| Wheel assemblies (foam tires, bearings) | vendor | 4 | $10 ($40) | |

## Power

| Part | Vendor | Qty | Unit cost |
|---|---|---|---|
| Li-Ion battery pack (5S 18650 ~19V, 3 Ah) | Battery Junction | 1 | $60 |
| BQ7850-class BMS for cell monitoring | Digikey | 1 | $18 |
| Buck converters (5V, 3.3V, 6V for motors) | Pololu | 3 | $10 ($30) |

## Chassis and mechanical

| Part | Vendor | Qty | Unit cost |
|---|---|---|---|
| Aluminum extrusion frame, 3D-printed brackets | in-house | 1 set | ~$60 |
| Cables, headers, standoffs, fasteners | McMaster / Amazon | - | ~$40 |

## Software and tools (free / open source)

- ROS2 Jazzy (Apache-2.0)
- slam_toolbox (Apache-2.0)
- robot_localization (BSD-3-Clause)
- Nav2 (Apache-2.0)
- m-explore-ros2 (BSD-3-Clause)
- sllidar_ros2 (BSD-3-Clause)
- ThreadX RTOS (Microsoft, MIT)
- nanopb (zlib License)
- Open3D (MIT)
- OpenCV (Apache-2.0)
- reportlab (BSD)

## BOM summary

| Category | Subtotal |
|---|---|
| Compute | ~$182 |
| Sensors | ~$270 |
| Motor drivetrain | ~$144 |
| Power | ~$108 |
| Chassis and mechanical | ~$100 |
| **Total BOM** | **~$804** (before custom PCB run) |
| With custom PCB (prototype + small-run) | **~$864 total** |

At the single-prototype scale, total hardware spend stays under $1,000 -
comfortably inside the capstone's "< $2,000" target and with headroom
for spares and iterations.
