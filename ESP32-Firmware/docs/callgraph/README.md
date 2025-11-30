# STAR ESP32 Firmware Call Graph Analysis

This directory contains comprehensive call graph analysis for the STAR ESP32 firmware project.

## Generated Content

### 1. GNU cflow Analysis (`cflow/`)
- `forward_callgraph.txt` - Forward call graph starting from `app_main()`
- `reverse_callgraph.txt` - Reverse call graph showing function callers
- `include_graph.txt` - Header file dependency analysis

### 2. Visual Call Graphs (`visual/`)
- `callgraph.dot` - Call graph in Graphviz DOT format
- `callgraph.png` - Visual call graph (PNG format)
- `callgraph.svg` - Vector call graph (SVG format)
- `callgraph.pdf` - Printable call graph (PDF format)

### 3. Multi-Level Analysis (`visual_levels/`)
- `callgraph_application.*` - Application layer view (high-level)
- `callgraph_framework.*` - Framework integration view 
- `callgraph_system.*` - System calls view
- `callgraph_complete.*` - Complete detailed view (low-level)

### 4. Doxygen Documentation (`doxygen/`)
- `html/index.html` - Full project documentation with integrated call/caller graphs
- Individual function pages include visual call graphs
- Cross-referenced source code with call relationships

## Quick Start

1. **High-level overview**: `visual_levels/callgraph_application.pdf`
2. **ESP-IDF integration**: `visual_levels/callgraph_framework.pdf` 
3. **Performance analysis**: `visual_levels/callgraph_system.pdf`
4. **Deep debugging**: `visual_levels/callgraph_complete.pdf`

## Understanding the Call Graphs

### Architecture Patterns Visible
1. **Dependency Inversion** - High-level modules depend on interfaces
2. **Bus Abstraction** - Unified interface for I2C, SPI, GPIO protocols
3. **Error Handling** - Centralized error management with retry logic
4. **Resource Management** - Pin validation and bus lifecycle management

### Maximum Call Depth
- **Application layer**: 4 levels deep
- **Complete analysis**: 7 levels deep
- **Most operations**: 3-5 levels (optimal for embedded systems)

Generated on: $(date)
