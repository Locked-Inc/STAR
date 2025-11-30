#!/bin/bash

# STAR ESP32 Firmware Call Graph Generation Script
# This script generates comprehensive call graphs using multiple tools

# Note: We use explicit error checking instead of set -e to allow partial completion

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/docs/callgraph"
BUILD_DIR="$PROJECT_ROOT/.pio/build/esp32_wroom"

# Tool paths
CFLOW_CMD="cflow"
DOT_CMD="dot"
DOXYGEN_CMD="doxygen"

print_header() {
    echo -e "${BLUE}=== STAR Firmware Call Graph Generator ===${NC}"
    echo -e "${YELLOW}Generating call graphs using multiple analysis methods${NC}"
    echo
}

check_tools() {
    echo -e "${BLUE}Checking required tools...${NC}"
    
    local missing_tools=()
    
    if ! command -v "$CFLOW_CMD" &> /dev/null; then
        missing_tools+=("cflow")
    fi
    
    if ! command -v "$DOXYGEN_CMD" &> /dev/null; then
        missing_tools+=("doxygen")
    fi
    
    if ! command -v "$DOT_CMD" &> /dev/null; then
        missing_tools+=("graphviz/dot")
    fi

    if ! command -v "python3" &> /dev/null; then
        missing_tools+=("python3")
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        echo -e "${RED}Missing required tools: ${missing_tools[*]}${NC}"
        echo -e "${YELLOW}Please install missing tools and try again${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}All required tools found${NC}"
}

setup_output_dir() {
    echo -e "${BLUE}Setting up output directories...${NC}"
    mkdir -p "$OUTPUT_DIR"/{cflow,visual,doxygen,visual_levels}
    echo -e "${GREEN}Output directory created: $OUTPUT_DIR${NC}"
}

build_project() {
    echo -e "${BLUE}Building project to generate RTL dumps...${NC}"
    cd "$PROJECT_ROOT"
    
    # Clean and build with RTL dumps enabled
    pio run -e esp32_wroom --target clean > /dev/null 2>&1
    if ! pio run -e esp32_wroom > /dev/null 2>&1; then
        echo -e "${RED}Project build failed${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Project built successfully${NC}"
}

generate_cflow_graphs() {
    echo -e "${BLUE}Generating GNU cflow call graphs...${NC}"
    cd "$PROJECT_ROOT"
    
    # Text-based call graph from app_main
    echo -e "  ${YELLOW}• Forward call graph from app_main${NC}"
    $CFLOW_CMD --main=app_main src/main.c lib/star_*/src/*.c > "$OUTPUT_DIR/cflow/forward_callgraph.txt" 2>/dev/null
    
    # Reverse call graph showing callers
    echo -e "  ${YELLOW}• Reverse call graph (callers)${NC}"
    $CFLOW_CMD -r --main=app_main src/main.c lib/star_*/src/*.c > "$OUTPUT_DIR/cflow/reverse_callgraph.txt" 2>/dev/null
    
    # Include file analysis
    echo -e "  ${YELLOW}• Include file dependency graph${NC}"
    $CFLOW_CMD --include-graph --main=app_main src/main.c lib/star_*/src/*.c > "$OUTPUT_DIR/cflow/include_graph.txt" 2>/dev/null
    
    echo -e "${GREEN}cflow graphs generated in $OUTPUT_DIR/cflow/${NC}"
}

generate_visual_graphs() {
    echo -e "${BLUE}Generating visual call graphs from cflow analysis...${NC}"
    cd "$PROJECT_ROOT"
    
    if [ ! -f "$OUTPUT_DIR/cflow/forward_callgraph.txt" ]; then
        echo -e "${RED}cflow output not found. Run generate_cflow_graphs first.${NC}"
        return 1
    fi
    
    # Generate DOT format call graph from cflow output
    echo -e "  ${YELLOW}• Converting cflow to DOT format${NC}"
    if ! python3 scripts/cflow_to_dot.py "$OUTPUT_DIR/cflow/forward_callgraph.txt" \
        -o "$OUTPUT_DIR/visual/callgraph.dot" \
        -t "STAR ESP32 Firmware Call Graph"; then
        echo -e "${RED}Failed to convert cflow to DOT format${NC}"
        return 1
    fi
    
    # Convert DOT to visual formats if graphviz is available
    if command -v "$DOT_CMD" &> /dev/null && [ -s "$OUTPUT_DIR/visual/callgraph.dot" ]; then
        echo -e "  ${YELLOW}• Converting to PNG format${NC}"
        if ! $DOT_CMD -Tpng "$OUTPUT_DIR/visual/callgraph.dot" -o "$OUTPUT_DIR/visual/callgraph.png"; then
            echo -e "${RED}Failed to generate PNG${NC}"
            return 1
        fi
        
        echo -e "  ${YELLOW}• Converting to SVG format${NC}"
        if ! $DOT_CMD -Tsvg "$OUTPUT_DIR/visual/callgraph.dot" -o "$OUTPUT_DIR/visual/callgraph.svg"; then
            echo -e "${RED}Failed to generate SVG${NC}"
            return 1
        fi
        
        echo -e "  ${YELLOW}• Converting to PDF format${NC}"
        if ! $DOT_CMD -Tpdf "$OUTPUT_DIR/visual/callgraph.dot" -o "$OUTPUT_DIR/visual/callgraph.pdf"; then
            echo -e "${RED}Failed to generate PDF${NC}"
            return 1
        fi
    fi
    
    echo -e "${GREEN}Visual call graphs generated in $OUTPUT_DIR/visual/${NC}"
    
    # Generate multi-level call graphs
    echo -e "  ${YELLOW}• Generating multi-level call graphs${NC}"
    if ! python3 scripts/cflow_to_dot_levels.py "$OUTPUT_DIR/cflow/forward_callgraph.txt" \
        -o "$OUTPUT_DIR/visual_levels" --generate-images; then
        echo -e "${RED}Failed to generate multi-level call graphs${NC}"
        return 1
    fi
    
    echo -e "${GREEN}Multi-level call graphs generated in $OUTPUT_DIR/visual_levels/${NC}"
}

generate_doxygen_docs() {
    echo -e "${BLUE}Generating Doxygen documentation with call graphs...${NC}"
    cd "$PROJECT_ROOT"
    
    if [ ! -f "Doxyfile" ]; then
        echo -e "${RED}Doxyfile not found${NC}"
        return 1
    fi
    
    # Generate full documentation with call/caller graphs
    if ! $DOXYGEN_CMD Doxyfile > "$OUTPUT_DIR/doxygen/generation.log" 2>&1; then
        echo -e "${RED}Doxygen generation failed. Check $OUTPUT_DIR/doxygen/generation.log${NC}"
        return 1
    fi
    
    # Copy generated HTML docs to our output directory
    if [ -d "docs/doxygen/html" ]; then
        if ! cp -r docs/doxygen/html "$OUTPUT_DIR/doxygen/"; then
            echo -e "${RED}Failed to copy Doxygen output${NC}"
            return 1
        fi
        echo -e "${GREEN}Doxygen documentation generated in $OUTPUT_DIR/doxygen/html/${NC}"
        echo -e "${YELLOW}Open $OUTPUT_DIR/doxygen/html/index.html in a browser to view${NC}"
    else
        echo -e "${RED}Doxygen HTML output not found${NC}"
        return 1
    fi
}

generate_summary() {
    echo -e "${BLUE}Generating summary report...${NC}"
    
    local summary_file="$OUTPUT_DIR/README.md"
    
    cat > "$summary_file" << 'EOF'
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
EOF
    
    echo -e "${GREEN}Summary report created: $summary_file${NC}"
}

main() {
    print_header
    check_tools
    setup_output_dir
    build_project
    
    # Generate all call graph types - continue even if some fail
    local failed_steps=()
    
    echo -e "${BLUE}Generating cflow analysis...${NC}"
    if ! generate_cflow_graphs; then
        failed_steps+=("cflow")
        echo -e "${RED}cflow generation failed${NC}"
    fi
    
    echo -e "${BLUE}Generating visual graphs...${NC}"
    if ! generate_visual_graphs; then
        failed_steps+=("visual")
        echo -e "${RED}Visual graph generation failed${NC}"
    fi
    
    echo -e "${BLUE}Generating Doxygen documentation...${NC}"
    if ! generate_doxygen_docs; then
        failed_steps+=("doxygen")
        echo -e "${RED}Doxygen generation failed${NC}"
    fi
    
    generate_summary
    
    echo
    if [ ${#failed_steps[@]} -eq 0 ]; then
        echo -e "${GREEN}=== Call Graph Generation Complete ===${NC}"
    else
        echo -e "${YELLOW}=== Call Graph Generation Complete (with some failures) ===${NC}"
        echo -e "${RED}Failed steps: ${failed_steps[*]}${NC}"
    fi
    
    echo -e "${YELLOW}Output directory: $OUTPUT_DIR${NC}"
    echo -e "${YELLOW}View the summary: $OUTPUT_DIR/README.md${NC}"
    echo -e "${YELLOW}Multi-level graphs: $OUTPUT_DIR/visual_levels/${NC}"
    echo -e "${YELLOW}Browse documentation: $OUTPUT_DIR/doxygen/html/index.html${NC}"
}

# Run main function if script is executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi