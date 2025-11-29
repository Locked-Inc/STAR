# STAR Robot Hardware Schematics

This directory contains the hardware design files for the STAR robot project, including PCB schematics, layouts, and manufacturing files.

## Contents

### Design Files
- **`*.kicad_pro`** - KiCad project files
- **`*.kicad_sch`** - Schematic files
- **`*.kicad_pcb`** - PCB layout files

### Manufacturing
- **`LiDAR Breakout Board Manufacture Items/`** - Production files for LiDAR breakout board
- **`STAR_GATE_DRIVER_EXPORTS/`** - Gate driver PCB manufacturing exports

### Components
- **`Symbols_and_Footprints/`** - Custom KiCad symbols and footprints
- **`MountingHole.pretty/`** - Mounting hole footprints
- **`Datasheets-and-references/`** - Component datasheets and reference designs

### Archives
- **`old/`** - Previous design iterations

## Key Hardware Components

### Main Boards
- **LiDAR Breakout Board** - Interface between TiM561 LiDAR and Raspberry Pi 5
- **Gate Driver Board** - Motor control electronics

### Supported Components
- **SICK TiM561 LiDAR** - Ethernet-based laser scanner
- **Raspberry Pi 5** - Main compute platform
- **ESP32 modules** - WiFi bridge and sensor interface
- **Power management circuits**
- **Motor drivers and control electronics**

## Design Tools

### Software Requirements
- **KiCad 8.0+** - Primary schematic and PCB design tool
- **KiCad libraries** - Standard component libraries

### Design Guidelines
- **Voltage levels**: 3.3V logic throughout
- **Power**: Designed for battery operation
- **Connectors**: Standard pitch connectors for easy assembly
- **EMI**: Proper ground planes and shielding considerations

## Getting Started

### Opening Projects
```bash
# Open in KiCad
kicad STAR_Robot.kicad_pro

# Or open individual schematics
kicad-cli sch export pdf STAR_Robot.kicad_sch
```

### Manufacturing Files
Manufacturing files (Gerbers, drill files, pick & place) are available in the respective export directories for each board design.

## License

Hardware designs are licensed under CERN Open Hardware License v2 - see individual project files for details.
