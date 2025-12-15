# Software Architecture Primer

Comprehensive technical reference documentation for the STAR robot control system software stack.

## Building the PDF

```bash
cd docs/software-architecture-primer
pdflatex main.tex
pdflatex main.tex  # Run twice for TOC
```

Or use latexmk:
```bash
latexmk -pdf main.tex
```

## Structure

```
software-architecture-primer/
|-- main.tex           # Main document entry point
|-- preamble.sty       # Package imports and styling
|-- .gitignore         # LaTeX build artifact exclusions
|-- README.md          # This file
+-- sections/
    |-- 01_glossary.tex
    |-- 02_executive_summary.tex
    |-- 03_protocol_analysis.tex
    |-- 04_system_architecture.tex
    |-- 05_slam_selection.tex
    |-- 06_ml_inference.tex
    |-- 07_cv_safety.tex
    |-- 08_battery_management.tex
    |-- 09_repository_structure.tex
    |-- 10_spring_boot_gateway.tex
    |-- 11_pwa_frontend.tex
    |-- 12_esp32_firmware.tex
    |-- 13_process_flow.tex
    |-- 14_deployment.tex
    |-- 15_implementation_checklist.tex
    |-- 16_technology_summary.tex
    +-- 17_references.tex
```

## Sections Overview

1. **Glossary** - Key terminology and definitions
2. **Executive Summary** - Architecture decisions overview
3. **Protocol Analysis** - gRPC + GraphQL hybrid comparison
4. **System Architecture** - Network topology and software stack
5. **SLAM Selection** - Algorithm comparison (SLAM Toolbox)
6. **ML Inference** - YOLOv8n person detection pipeline
7. **CV Safety** - Computer vision safety system
8. **Battery Management** - Multi-threshold battery system
9. **Repository Structure** - Project directory layout
10. **Spring Boot Gateway** - Kotlin backend implementation
11. **PWA Frontend** - Vue 3 + TypeScript implementation
12. **ESP32 Firmware** - TCP module additions
13. **Process Flow** - Control flow diagrams
14. **Deployment** - systemd and Envoy configuration
15. **Implementation Checklist** - Phase-by-phase tasks
16. **Technology Summary** - Complete tech stack table
17. **References** - Documentation links
