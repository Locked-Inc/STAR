# STAR Documentation

This directory contains the STAR project's technical documentation -- prose chapters,
API references, and a combined volume that merges everything into one PDF.

## Generated PDFs

| PDF | Description | Pages | Size |
|-----|-------------|-------|------|
| `star_prose.pdf` | System architecture, protocols, style guides (13 LaTeX chapters) | ~223 | 1.4 MB |
| `star_firmware_api.pdf` | RX72N C firmware API reference (all headers + call graphs) | ~2800 | 35 MB |
| `star_ros2_api.pdf` | ROS2 C++ API reference | ~160 | 1.1 MB |
| `star_gateway_api.pdf` | Go gateway service API reference | -- | 1.5 MB |
| `star_complete.pdf` | Everything above combined with a unified TOC | ~3100 | 16 MB |

## Prerequisites

Install the required tools before building:

```bash
# TeX Live (pdflatex)
sudo apt install texlive-latex-extra texlive-fonts-recommended texlive-science

# Typst (>= 0.14)
cargo install typst-cli
# or: curl -fsSL https://typst.community | sh

# Doxygen (>= 1.9)
sudo apt install doxygen

# Go (>= 1.21) -- only for gateway API
# Already installed if you use the devcontainer

# Graphviz -- for Doxygen call graphs
sudo apt install graphviz
```

Additional LaTeX packages that may need manual installation via `tlmgr`:

```bash
tlmgr install pdfpages varwidth xltabular tabularray stackengine etoc \
  changepage fancyvrb enumitem alphalph ulem listofitems etoolbox \
  ninecolors sectsty natbib newunicodechar psnfss iftex infwarerr \
  kvoptions listings fancyhdr geometry hyperref courier caption
```

## Building Individual PDFs

### 1. Prose PDF (`star_prose.pdf`)

Compiles the 13 LaTeX chapters (architecture, protocols, style guides) directly
from `sections/*.tex` using preamble.sty for custom formatting (colored boxes,
TikZ diagrams, code listings).

```bash
cd docs/
make                           # runs pdflatex twice for TOC resolution
cp star_documentation.pdf star_prose.pdf
```

Or manually:

```bash
cd docs/
pdflatex -interaction=nonstopmode -halt-on-error star_documentation.tex
pdflatex -interaction=nonstopmode -halt-on-error star_documentation.tex
cp star_documentation.pdf star_prose.pdf
```

### 2. Firmware API PDF (`star_firmware_api.pdf`)

Generates API docs from all RX72N C firmware headers (533 source files). Uses
a three-stage pipeline: Doxygen (source -> XML) -> star-docs (XML -> Typst) ->
Typst (Typst -> PDF).

**Why not Doxygen LaTeX?** The firmware codebase is too large -- pdfTeX hits its
32768 register limit and crashes. The Typst pipeline handles arbitrarily large
documents.

```bash
# Step 1: Generate Doxygen XML from firmware source
cd e2-studio-star-rx72n-firmware/
doxygen Doxyfile                # produces docs/doxygen/xml/

# Step 2: Generate Typst from Doxygen XML (using star-docs)
cd tools/star-docs/
cargo build --release
./target/release/star-docs generate-typst --subsystem rx72n-firmware

# Step 3: Fix known Typst escaping issues in generated output
cd /workspaces/STAR
python3 tools/fix_typst_api.py docs/typst-gen/api_rx72n_firmware.typ
python3 tools/fix_typst_brackets.py docs/typst-gen/api_rx72n_firmware.typ
python3 tools/fix_typst_math.py docs/typst-gen/api_rx72n_firmware.typ

# Step 4: Compile Typst to PDF
typst compile docs/typst-gen/firmware_standalone.typ docs/star_firmware_api.pdf
```

The fix scripts handle escaping issues in the star-docs output:
- `fix_typst_api.py` -- escapes `*` and `_` characters from C source (pointers, identifiers)
- `fix_typst_brackets.py` -- escapes `[`/`]` inside `#doc-xxx[...]` content blocks
- `fix_typst_math.py` -- converts broken LaTeX math to plain text

### 3. ROS2 API PDF (`star_ros2_api.pdf`)

Generates API docs from the ROS2 C++ packages (19 files). Small enough for
Doxygen's built-in LaTeX pipeline.

```bash
# Step 1: Generate Doxygen LaTeX
cd star-ros2/
doxygen Doxyfile                # produces docs/doxygen/latex/

# Step 2: Compile LaTeX to PDF
cd docs/doxygen/latex/
make                            # runs pdflatex + makeindex

# Step 3: Copy result
cp star-ros2/docs/doxygen/latex/refman.pdf docs/star_ros2_api.pdf
```

### 4. Gateway API PDF (`star_gateway_api.pdf`)

Generates API docs from Go source using `go doc` output converted to Typst.

```bash
# Step 1: Generate Typst from Go source
cd /workspaces/STAR
python3 tools/gen_go_api_typst.py        # produces docs/gateway_api.typ

# Step 2: Compile Typst to PDF
typst compile docs/gateway_api.typ docs/star_gateway_api.pdf
```

## Building the Combined PDF (`star_complete.pdf`)

The combined document merges all prose chapters and API reference PDFs into a
single volume with a unified table of contents. It uses pdfLaTeX with the
`pdfpages` package to embed the API PDFs.

**You must build all 4 individual PDFs first** -- the combined build includes
them via `\includepdf`.

```bash
cd docs/

# Verify all API PDFs exist
ls star_firmware_api.pdf star_gateway_api.pdf star_ros2_api.pdf star_prose.pdf

# Build combined PDF (two passes for TOC)
pdflatex -interaction=nonstopmode -halt-on-error star_complete.tex
pdflatex -interaction=nonstopmode -halt-on-error star_complete.tex
```

The combined PDF structure:

```
Part I:   System Architecture & Protocols (7 chapters)
Part II:  Style Guides (4 chapters)
Part III: Gateway & ROS2 (2 chapters)
Part IV:  API Reference -- RX72N Firmware (C)      [embedded PDF]
Part V:   API Reference -- Go Gateway              [embedded PDF]
Part VI:  API Reference -- ROS2 (C++)              [embedded PDF]
```

## Directory Structure

```
docs/
  Makefile                  # Build system for prose PDF
  preamble.sty              # LaTeX preamble (colors, boxes, TikZ, listings)
  star_documentation.tex    # Main prose document (includes sections/*.tex)
  star_complete.tex         # Combined document (prose + API PDFs)
  sections/                 # LaTeX chapter source files
    01_nanopb_protocol.tex
    02_protobuf_schemas.tex
    03_hardware_pinout.tex
    04_style_guide.tex
    05_c_style_guide.tex
    06_nasa_power_of_10.tex
    07_gateway_architecture.tex
    09_usb_cdc_protocol.tex
    10_ros2_integration.tex
    11_ros2_cpp_style_guide.tex
    12_communication_stack.tex
    13_transport_failover.tex
    14_dual_channel_arbitration.tex
  typst-gen/                # Auto-generated Typst files (from star-docs)
    _template.typ           # Shared Typst template (styling, doc blocks)
    firmware_standalone.typ # Standalone wrapper for firmware API
    api_rx72n_firmware.typ  # Generated firmware API (~88K lines)
    graphs/                 # Call graph SVGs from Doxygen
  gateway_api.typ           # Generated gateway API Typst file
```

## Troubleshooting

**pdflatex: "File not found" for API PDFs**
Build the individual PDFs first. The combined document expects `star_firmware_api.pdf`,
`star_gateway_api.pdf`, and `star_ros2_api.pdf` in the same directory.

**Typst: "unknown variable: func-sig"**
The API Typst file needs `#import "_template.typ": *` at line 1. The fix scripts
add this automatically. If regenerating from star-docs, re-run the fix scripts.

**Doxygen LaTeX: "Bad register code (32768)"**
The RX72N firmware is too large for pdfTeX. Use the star-docs Typst pipeline
instead (see "Firmware API PDF" above).

**Missing LaTeX packages**
Install via `tlmgr install <package-name>`. Check `star_complete.log` for the
exact missing package name.
