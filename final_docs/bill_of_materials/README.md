# Bill of Materials - ESET 420 Capstone Submission

## Primary Deliverable

`STAR_MCU_BOM.csv` -- canonical BOM for the STAR custom motherboard
PCB (4-layer, Renesas RX72N, motor drivers, power distribution,
IMU + sonar interfaces). Generated directly from the KiCad
schematic source using the KiCad 10.0.1 command line.

Columns: Reference, Value, Footprint, QUANTITY, DNP, MPN,
Manufacturer, Description. Grouped by Value+Footprint+MPN,
DNP-excluded, 46 component groups.

## Per-Subcircuit BOMs

The board is authored as several schematic blocks. Each block has a
standalone CSV in `per_subcircuit/`, useful when sourcing a single
subsystem independently:

- `STAR_AFE_BOM.csv` -- battery monitor / analog front end
- `STAR_BUCK_BOOST_CHARGER_BOM.csv` -- battery charger
- `STAR_FUEL_GAUGE_BOM.csv` -- battery fuel gauge
- `STAR_GATE_DRIVER_BOM.csv` -- MOSFET gate driver
- `STAR_LIDAR_BREAKOUT_BOM.csv` -- RPLiDAR breakout connector
- `STAR_MCU_BOM.csv` -- the RX72N MCU section
- `STAR_MOTOR_DRIVER_BOM.csv` -- DRV8263H quad H-bridge
- `STAR_P3V3_BUCK_BOM.csv` -- 3.3 V buck regulator
- `STAR_P5V0_BUCK_BOM.csv` -- 5 V buck regulator
- `STAR_P6V0_BUCK_BOM.csv` -- 6 V buck regulator for motors
- `STAR_USB-C_BOM.csv` -- USB-C connector + protection

## Human-Readable Summary

`BOM_summary.md` is a mirror of `final_capstone/bom.md`. It adds
cost totals, manufacturer notes, and the rough buy-list view that
was presented to the review committee.

## Regeneration Command

To reproduce `STAR_MCU_BOM.csv` from the KiCad source:

```
kicad-cli sch export bom \
  -o final_docs/bill_of_materials/STAR_MCU_BOM.csv \
  --fields "Reference,Value,Footprint,QUANTITY,DNP,MPN,Manufacturer,Description" \
  --group-by "Value,Footprint,MPN" \
  --exclude-dnp \
  Schematic/STAR_MCU.kicad_sch
```

Tested with KiCad CLI 10.0.1 on macOS.

## Authoritative Source

Master schematic: `Schematic/STAR_MCU.kicad_sch`.
Master PCB: `Schematic/STAR_MCU.kicad_pcb`.

Any BOM edit must flow back through the KiCad schematic (symbol
fields), not through this CSV in isolation.

## Team

Locked Inc. (Texas A&M ESET Senior Capstone, Spring 2026).
See the SDD or SSUM title page for the full team roster.
