# Call Graph Generation Guide for STAR ESP32 Firmware

This comprehensive guide provides step-by-step instructions for generating and analyzing call graphs for the STAR ESP32 firmware project.

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Installation](#installation)
4. [Usage](#usage)
5. [Understanding Call Graphs](#understanding-call-graphs)
6. [Troubleshooting](#troubleshooting)
7. [Advanced Usage](#advanced-usage)

## Overview

Call graphs are essential tools for understanding code execution flow, especially in embedded systems where understanding function relationships, stack usage, and execution paths is critical for:

- **Performance Analysis**: Identifying bottlenecks and optimization opportunities
- **Memory Management**: Understanding stack depth and function call overhead
- **Architecture Validation**: Verifying dependency injection patterns
- **Debugging**: Tracing execution paths and finding root causes
- **Code Reviews**: Understanding component interactions

The STAR firmware uses multiple complementary approaches:

1. **GNU cflow**: Static source code analysis
2. **Egypt**: Binary analysis from compiled RTL dumps
3. **Doxygen**: Documentation generation with integrated call graphs

## Prerequisites

### System Requirements

- macOS, Linux, or Windows with WSL
- GCC toolchain (provided by ESP-IDF/PlatformIO)
- Python 3.6+ (for PlatformIO)
- Git

### Build Environment

Ensure you have a working ESP32 development environment:

```bash
# Verify PlatformIO is installed
pio --version

# Verify project builds
cd /path/to/ESP32-Firmware
pio run -e esp32_wroom
```

## Installation

### macOS Installation

```bash
# Install package manager if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required tools
brew install cflow doxygen graphviz

# Verify installations
cflow --version
doxygen --version
dot -V
```

### Linux (Ubuntu/Debian) Installation

```bash
# Update package list
sudo apt-get update

# Install required tools
sudo apt-get install -y cflow doxygen graphviz

# Verify installations
cflow --version
doxygen --version
dot -V
```

### Linux (CentOS/RHEL/Fedora) Installation

```bash
# For CentOS/RHEL (with EPEL)
sudo yum install -y epel-release
sudo yum install -y cflow doxygen graphviz

# For Fedora
sudo dnf install -y cflow doxygen graphviz

# Verify installations
cflow --version
doxygen --version
dot -V
```

### Windows (WSL) Installation

```bash
# Open WSL terminal and update
sudo apt-get update

# Install required tools
sudo apt-get install -y cflow doxygen graphviz

# Verify installations
cflow --version
doxygen --version
dot -V
```

### Egypt Tool Installation (Manual)

Egypt is not available in standard package managers, so it must be installed manually:

```bash
# Download and extract
curl -O https://www.gson.org/egypt/download/egypt-1.11.tar.gz
tar -xzf egypt-1.11.tar.gz

# Copy to project directory
cp egypt-1.11/egypt ./
chmod +x egypt

# Clean up
rm -rf egypt-1.11 egypt-1.11.tar.gz

# Verify installation
./egypt --help
```

### Perl Requirements (for Egypt)

Egypt requires Perl, which is typically pre-installed on Unix systems:

```bash
# Verify Perl installation
perl --version

# If not installed, install Perl:
# macOS: brew install perl
# Ubuntu: sudo apt-get install perl
# CentOS: sudo yum install perl
```

## Usage

### Quick Start

1. **Clone and setup the project**:
   ```bash
   git clone <repository-url>
   cd ESP32-Firmware
   ```

2. **Generate all call graphs**:
   ```bash
   ./scripts/generate_callgraph.sh
   ```

3. **View results**:
   ```bash
   # Text-based analysis
   cat docs/callgraph/cflow/forward_callgraph.txt
   
   # Visual graphs
   open docs/callgraph/egypt/callgraph.png
   
   # Interactive documentation
   open docs/callgraph/doxygen/html/index.html
   ```

### Step-by-Step Manual Generation

#### 1. GNU cflow Analysis

Generate text-based call graphs from source code:

```bash
# Forward call graph (what functions does app_main call?)
cflow --main=app_main src/main.c lib/star_*/src/*.c > forward_callgraph.txt

# Reverse call graph (what functions call app_main?)
cflow -r --main=app_main src/main.c lib/star_*/src/*.c > reverse_callgraph.txt

# Include dependency graph
cflow --include-graph src/main.c lib/star_*/src/*.c > include_graph.txt

# Generate cross-reference
cflow --xref src/main.c lib/star_*/src/*.c > cross_reference.txt
```

#### 2. Egypt Binary Analysis

Generate visual call graphs from compiled code:

```bash
# Build project with RTL dumps enabled
pio run -e esp32_wroom

# Find RTL dump files
find . -name "*.expand" -path "*/src/*" -o -name "*.expand" -path "*/star_*/*"

# Generate DOT format graph
find . -name "*.expand" -path "*/src/*" -o -name "*.expand" -path "*/star_*/*" | ./egypt > callgraph.dot

# Convert to various formats
dot -Tpng callgraph.dot -o callgraph.png    # PNG image
dot -Tsvg callgraph.dot -o callgraph.svg    # SVG vector
dot -Tpdf callgraph.dot -o callgraph.pdf    # PDF document
dot -Tps callgraph.dot -o callgraph.ps      # PostScript
```

#### 3. Doxygen Documentation

Generate comprehensive documentation with call graphs:

```bash
# Generate documentation
doxygen Doxyfile

# Documentation will be in docs/doxygen/html/
# Open main page: docs/doxygen/html/index.html
```

### Automated Script Usage

The provided script offers several options:

```bash
# Generate all call graph types
./scripts/generate_callgraph.sh

# The script automatically:
# 1. Checks for required tools
# 2. Sets up output directories
# 3. Builds the project with RTL dumps
# 4. Generates cflow graphs
# 5. Generates Egypt graphs
# 6. Generates Doxygen documentation
# 7. Creates a summary report
```

## Understanding Call Graphs

### Reading cflow Output

The cflow output shows a hierarchical tree of function calls:

```
app_main() <void app_main (void) at src/main.c:48>:
    pin_validator_get_interface() <void pin_validator_get_interface (star_pin_interface_t *iface) at lib/star_pin_validator/src/star_pin_validator.c:487>:
        iface_register_pin() <esp_err_t iface_register_pin (void *ctx, int pin, const char *description, bool shared) at lib/star_pin_validator/src/star_pin_validator.c:463>:
            star_register_pin() <esp_err_t star_register_pin (gpio_num_t gpio_num, const char *desc, bool can_be_shared) at lib/star_pin_validator/src/star_pin_validator.c:51>:
                init_mutex() <esp_err_t init_mutex (void) at lib/star_pin_validator/src/star_pin_validator.c:25>:
```

**Key elements:**
- Function name and signature
- Source file and line number
- Indentation shows call depth
- Return types and parameters

### Reading Egypt Visual Graphs

Egypt generates DOT format graphs that can be visualized:

- **Nodes**: Represent functions
- **Edges**: Represent function calls
- **Arrows**: Show call direction
- **Clusters**: Group related functions

### Understanding STAR Architecture Through Call Graphs

The call graphs reveal several architectural patterns:

#### 1. Dependency Injection Pattern
```
app_main()
├── pin_validator_get_interface()      # Get pin interface
├── star_error_interface_create_default()  # Get error interface
├── star_bus_manager_init()            # Inject interfaces
└── sensor initialization...           # Use injected interfaces
```

#### 2. Bus Abstraction Layer
```
Sensor Driver
├── star_bus_manager_find_bus()
├── star_bus_i2c_write()
└── actual hardware calls
```

#### 3. Error Handling Flow
```
Operation
├── error_handler_record_error()
├── error_handler_can_retry()
└── retry logic or failure handling
```

### Common Call Graph Patterns

1. **Initialization Phase**: Setup interfaces and components
2. **Configuration Phase**: Configure buses and sensors  
3. **Runtime Phase**: Main execution loop
4. **Cleanup Phase**: Resource deallocation

## Troubleshooting

### Common Issues and Solutions

#### 1. "cflow: command not found"
```bash
# Install cflow
brew install cflow  # macOS
sudo apt-get install cflow  # Linux
```

#### 2. "egypt: command not found"
```bash
# Make sure egypt is executable and in the project directory
chmod +x egypt
ls -la egypt
```

#### 3. "No RTL dump files found"
```bash
# Verify build includes RTL flag
grep "fdump-rtl-expand" platformio.ini

# Clean and rebuild
pio run -t clean
pio run -e esp32_wroom
```

#### 4. "Doxygen: Output directory does not exist"
```bash
# Create output directory
mkdir -p docs/doxygen
```

#### 5. "Graph is too large for rendering"
```bash
# Use SVG format for large graphs
dot -Tsvg callgraph.dot -o callgraph.svg

# Or increase DPI limit
dot -Gdpi=72 -Tpng callgraph.dot -o callgraph.png
```

### Performance Issues

For large codebases:

1. **Limit scope**: Use specific source files instead of wildcards
2. **Use filters**: Focus on specific functions or modules
3. **Generate smaller graphs**: Use modular analysis

Example limited scope analysis:
```bash
# Analyze only bus manager
cflow --main=star_bus_manager_init lib/star_bus/src/star_bus_manager.c

# Analyze only sensor drivers  
cflow lib/star_sensor_*/src/*.c
```

## Advanced Usage

### Custom cflow Options

```bash
# Include external functions
cflow --include-graph --all src/main.c

# Reverse engineering mode
cflow --reverse src/main.c

# Generate cross-reference table
cflow --xref src/main.c

# Number functions
cflow --number src/main.c

# Show file locations
cflow --print-level src/main.c
```

### Egypt Filtering

```bash
# Filter by specific functions
grep "app_main\|star_bus" callgraph.dot > filtered_graph.dot

# Remove ESP-IDF noise
grep -v "esp_\|ESP_\|xTaskCreate\|vTaskDelay" callgraph.dot > clean_graph.dot
```

### Doxygen Customization

Edit `Doxyfile` to customize output:

```bash
# Enable/disable different graph types
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
COLLABORATION_GRAPH    = YES

# Set graph format
DOT_IMAGE_FORMAT       = svg

# Include private functions
EXTRACT_PRIVATE        = YES
EXTRACT_STATIC         = YES
```

### Integration with Development Workflow

#### Git Hooks

Add call graph generation to git hooks:

```bash
#!/bin/sh
# .git/hooks/pre-commit
echo "Generating call graphs..."
./scripts/generate_callgraph.sh
git add docs/callgraph/
```

#### CI/CD Integration

Include in GitHub Actions or similar:

```yaml
# .github/workflows/documentation.yml
- name: Generate Call Graphs
  run: |
    sudo apt-get install cflow doxygen graphviz
    ./scripts/generate_callgraph.sh
    
- name: Deploy Documentation
  uses: peaceiris/actions-gh-pages@v3
  with:
    github_token: ${{ secrets.GITHUB_TOKEN }}
    publish_dir: ./docs/callgraph/doxygen/html
```

### Performance Analysis

Use call graphs for performance analysis:

1. **Identify hot paths**: Frequently called functions
2. **Stack depth analysis**: Deep call chains may cause stack overflow
3. **Cyclic dependencies**: Circular calls can indicate design issues
4. **Dead code detection**: Unreachable functions

### Memory Analysis

Estimate stack usage from call graphs:

1. **Calculate maximum call depth**
2. **Estimate local variable sizes**
3. **Identify recursive functions**
4. **Plan stack size requirements**

This guide provides comprehensive coverage for call graph generation and analysis. The tools and techniques described will help you understand, debug, and optimize the STAR ESP32 firmware effectively.