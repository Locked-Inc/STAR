# Electrical Schematics and PCB Design - ESET 420 Capstone Submission

All artifacts in this directory were generated directly from the
KiCad 10.0.1 source project at `Schematic/STAR_MCU.*` via
`kicad-cli`. No GUI-side manual export.

## Deliverables

| File | Size | What it is | Open it to see |
|---|---|---|---|
| `STAR_MCU_schematic.pdf` | 699 KB | Schematic sheet | All signals, nets, and symbol values for the board |
| `STAR_MCU_PCB.pdf` | 3.8 MB | Multi-page layer PDF | One PDF page per copper, silkscreen, soldermask, fab, and edge-cut layer; border and title block on each |
| `STAR_MCU_PCB.step` | 7.2 MB | 3D STEP model | The populated board as a neutral-format solid model for mechanical integration |
| `STAR_MCU_gerbers.zip` | 808 KB | Fabrication archive | Gerber RS-274X per copper/mask/silk/edge layer + Excellon drill file (`.drl`) |
| `STAR_MCU_erc.rpt` | 90 KB | Electrical Rule Check | KiCad's authoritative ERC run on the schematic |
| `gerbers/` | - | Unzipped gerber staging | Source of the `.zip` above (kept for convenience) |

## Board Summary

- **Stackup:** 6-layer (F.Cu, In1.Cu, In2.Cu, In3.Cu, In4.Cu, B.Cu)
- **Microcontroller:** Renesas RX72N (R5F572NNHDFB, 144-pin)
- **Motor drivers:** 4x Texas Instruments DRV8263H dual H-bridge
- **Power rails:** 3.3 V, 5.0 V, and 6.0 V buck regulators from
  battery input; USB-C input with protection
- **Battery management:** integrated charger, fuel gauge, and AFE
- **Peripheral interfaces:** LiDAR breakout connector, IMU, sonar,
  debug UART over USB-C

## Reviewer Reading Order

1. Open `STAR_MCU_schematic.pdf` for the signal-level walkthrough
   of every subsystem.
2. Open `STAR_MCU_PCB.pdf` to review each copper layer, silkscreen,
   and the edge cut.
3. Open `STAR_MCU_PCB.step` in FreeCAD, SolidWorks, or Fusion 360
   if you want the 3D model for mechanical integration.
4. Extract `STAR_MCU_gerbers.zip` if you want to send the board
   to a fab (JLCPCB, PCBWay, OSH Park).
5. Read `STAR_MCU_erc.rpt` for the electrical-rule-check output.

## Regeneration

Every artifact here regenerates from the KiCad source:

```bash
# Schematic PDF
kicad-cli sch export pdf \
  -o final_docs/electrical_schematics_pcb/STAR_MCU_schematic.pdf \
  Schematic/STAR_MCU.kicad_sch

# PCB multi-page PDF (6-layer stackup)
kicad-cli pcb export pdf --mode-multipage \
  --layers "F.Cu,In1.Cu,In2.Cu,In3.Cu,In4.Cu,B.Cu,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts,F.Fab,B.Fab" \
  --include-border-title \
  -o final_docs/electrical_schematics_pcb/STAR_MCU_PCB.pdf \
  Schematic/STAR_MCU.kicad_pcb

# 3D STEP (substitute-models on; DNP and unspecified excluded)
kicad-cli pcb export step \
  -o final_docs/electrical_schematics_pcb/STAR_MCU_PCB.step \
  --subst-models --no-unspecified --no-dnp \
  Schematic/STAR_MCU.kicad_pcb

# Gerbers + Excellon drill + zip
kicad-cli pcb export gerbers -o final_docs/electrical_schematics_pcb/gerbers/ Schematic/STAR_MCU.kicad_pcb
kicad-cli pcb export drill   -o final_docs/electrical_schematics_pcb/gerbers/ Schematic/STAR_MCU.kicad_pcb
( cd final_docs/electrical_schematics_pcb && zip -rq STAR_MCU_gerbers.zip gerbers/ )
```

Tested on macOS with KiCad CLI 10.0.1.

## Known Warnings (not blocking)

- STEP export logs two missing 3D model files for the MOSFET
  `SSM6J507NU,LF.stp` and the RX72N `R5F572NNHDFB#30.stp`. These
  are purely cosmetic in the 3D viewer; the PCB outline, copper,
  and mounting holes are correct and usable for mechanical
  integration. The STEP file is otherwise valid.

## Authoritative Source

- Schematic: `Schematic/STAR_MCU.kicad_sch`
- PCB: `Schematic/STAR_MCU.kicad_pcb`
- Project: `Schematic/STAR_MCU.kicad_pro`

Any electrical-design change must flow through KiCad, not through
these exported artifacts.

## Team

Locked Inc. (Texas A&M ESET Senior Capstone, Spring 2026).
Hardware authors: Brighton Sikarskie (embedded + schematic),
Jeremie Hockey (PCB design and layout), with schematic review from
Shawn Ciaciura and Jared Bartz. See the SDD or SSUM for the full
team roster.
