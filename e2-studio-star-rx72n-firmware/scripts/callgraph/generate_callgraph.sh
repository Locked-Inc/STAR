#!/bin/bash
# RX72N Firmware Call Graph Generation Script
# Usage: ./generate_callgraph.sh [options]
#
# Generates call graphs from the RX72N firmware source using three
# complementary methods: cflow (source-level), RTL+egypt (compiler-verified),
# and Doxygen (browsable HTML documentation).

set -e  # Exit on any error
set +H  # Disable history expansion

# =============================================================================
# Configuration
# =============================================================================

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project root (relative to this script)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Output directory
OUTPUT_DIR="$PROJECT_ROOT/docs/callgraph"

# Source directories to scan
SRC_DIRS=("src" "libs")

# Directories to exclude from analysis
EXCLUDE_DIRS=(
  "libs/threadx"
  "libs/rx_nanopb/nanopb"
  "src/smc_gen"
  "src/boot/smc_generated"
  "tests"
)

# External function prefixes to exclude from graphs
EXTERNAL_PREFIXES=(
  "tx_"
  "pb_"
  "R_BSP_"
  "R_Config_"
  "__builtin_"
  "memcpy"
  "memset"
  "memcmp"
  "strlen"
  "strcmp"
  "strncmp"
  "strcpy"
  "strncpy"
  "snprintf"
  "printf"
  "puts"
  "putchar"
)

# Subsystem definitions (module groupings)
declare -A SUBSYSTEMS
SUBSYSTEMS[motor]="rx_motor rx_pid rx_drv8263 rx_encoder"
SUBSYSTEMS[comm]="rx_comm_manager rx_spi_comm rx_usb_comm rx_usb rx_frame rx_frame_ascii"
SUBSYSTEMS[protocol]="rx_nanopb rx_crc rx_fec rx_harq"
SUBSYSTEMS[sensors]="rx_ds18b20 rx_hcsr04 rx_obstacle_detect"
SUBSYSTEMS[bus]="rx_bus"
SUBSYSTEMS[core]="rx_core rx_hal rx_infrastructure"

# Default values
METHOD="cflow"
SCOPE="full"
MODULE=""
SUBSYSTEM=""
FUNCTION=""
TASK=""
DEPTH=0
FORMAT="svg"
VERBOSE=false
INCLUDE_THREADX_EDGES=true
INCLUDE_ISR_EDGES=true
LAYOUT="dot"
RANKDIR="TB"

# =============================================================================
# Utility Functions
# =============================================================================

print_status() {
  echo -e "${BLUE}[INFO]${NC} $1" >&2
}

print_success() {
  echo -e "${GREEN}[OK]${NC} $1" >&2
}

print_warning() {
  echo -e "${YELLOW}[WARN]${NC} $1" >&2
}

print_error() {
  echo -e "${RED}[ERROR]${NC} $1" >&2
}

print_step() {
  echo -e "${CYAN}[STEP]${NC} $1" >&2
}

usage() {
  cat <<'USAGE'
RX72N Firmware Call Graph Generator

Usage: ./generate_callgraph.sh [options]

Methods:
  --method=cflow          Source-level analysis via GNU cflow (default)
  --method=rtl            Compiler RTL dump via GCC + egypt
  --method=doxygen        Doxygen HTML with per-function call graphs

Scope options (cflow/rtl):
  --scope=full            Full project call graph (default)
  --module=NAME           Single library module (e.g., rx_pid)
  --subsystem=NAME        Subsystem group (motor, comm, protocol, sensors, bus, core)
  --task=NAME             Task-rooted graph (e.g., motor_control)
  --function=NAME         Single function as root node

Filtering:
  --depth=N               Max call depth (0 = unlimited, default)
  --no-threadx-edges      Omit manual ThreadX dispatch edges
  --no-isr-edges          Omit manual ISR vector edges

Output:
  --format=svg            SVG output (default, recommended for browsers)
  --format=png            PNG raster output
  --format=pdf            PDF vector output
  --format=dot            Raw DOT source (no rendering)
  --rankdir=TB            Top-to-bottom layout (default)
  --rankdir=LR            Left-to-right layout
  --layout=dot            Hierarchical layout (default, best for call trees)
  --layout=sfdp           Force-directed layout (better for large graphs)
  --output-dir=DIR        Output directory (default: docs/callgraph/)

General:
  -v, --verbose           Verbose output
  -h, --help              Show this help message

Examples:
  ./generate_callgraph.sh --method=cflow --scope=full
  ./generate_callgraph.sh --method=cflow --module=rx_pid
  ./generate_callgraph.sh --method=cflow --subsystem=motor
  ./generate_callgraph.sh --method=cflow --task=motor_control
  ./generate_callgraph.sh --method=cflow --function=rx_pid_compute --depth=3
  ./generate_callgraph.sh --method=rtl --scope=full
  ./generate_callgraph.sh --method=doxygen

Subsystems:
  motor     rx_motor, rx_pid, rx_drv8263, rx_encoder
  comm      rx_comm_manager, rx_spi_comm, rx_usb_comm, rx_usb, rx_frame, rx_frame_ascii
  protocol  rx_nanopb, rx_crc, rx_fec, rx_harq
  sensors   rx_ds18b20, rx_hcsr04, rx_obstacle_detect
  bus       rx_bus
  core      rx_core, rx_hal, rx_infrastructure
USAGE
}

# =============================================================================
# Argument Parsing
# =============================================================================

parse_args() {
  while [[ $# -gt 0 ]]; do
    case $1 in
      --method=*)
        METHOD="${1#*=}"
        shift
        ;;
      --scope=*)
        SCOPE="${1#*=}"
        shift
        ;;
      --module=*)
        MODULE="${1#*=}"
        SCOPE="module"
        shift
        ;;
      --subsystem=*)
        SUBSYSTEM="${1#*=}"
        SCOPE="subsystem"
        shift
        ;;
      --task=*)
        TASK="${1#*=}"
        SCOPE="task"
        shift
        ;;
      --function=*)
        FUNCTION="${1#*=}"
        SCOPE="function"
        shift
        ;;
      --depth=*)
        DEPTH="${1#*=}"
        shift
        ;;
      --format=*)
        FORMAT="${1#*=}"
        shift
        ;;
      --rankdir=*)
        RANKDIR="${1#*=}"
        shift
        ;;
      --layout=*)
        LAYOUT="${1#*=}"
        shift
        ;;
      --output-dir=*)
        OUTPUT_DIR="${1#*=}"
        shift
        ;;
      --no-threadx-edges)
        INCLUDE_THREADX_EDGES=false
        shift
        ;;
      --no-isr-edges)
        INCLUDE_ISR_EDGES=false
        shift
        ;;
      -v|--verbose)
        VERBOSE=true
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        print_error "Unknown option: $1"
        usage
        exit 1
        ;;
    esac
  done
}

# =============================================================================
# Dependency Checks
# =============================================================================

check_dependencies() {
  local missing=false

  case "$METHOD" in
    cflow)
      if ! command -v cflow &>/dev/null; then
        print_error "cflow not found. Install: sudo apt-get install -y cflow"
        missing=true
      fi
      if ! command -v gcc &>/dev/null; then
        print_error "gcc not found (needed for preprocessing). Install: sudo apt-get install -y gcc"
        missing=true
      fi
      ;;
    rtl)
      if ! command -v gcc &>/dev/null; then
        print_error "gcc not found. Install: sudo apt-get install -y gcc"
        missing=true
      fi
      if ! command -v egypt &>/dev/null; then
        print_warning "egypt not found. Install: sudo cpan Graph::Easy; or download from https://github.com/ltratt/egypt"
        print_warning "Falling back to manual RTL parsing"
      fi
      ;;
    doxygen)
      if ! command -v doxygen &>/dev/null; then
        print_error "doxygen not found. Install: sudo apt-get install -y doxygen"
        missing=true
      fi
      ;;
    *)
      print_error "Unknown method: $METHOD (use cflow, rtl, or doxygen)"
      exit 1
      ;;
  esac

  # Graphviz is needed for all methods except raw DOT output
  if [[ "$FORMAT" != "dot" ]]; then
    if ! command -v dot &>/dev/null; then
      print_error "Graphviz (dot) not found. Install: sudo apt-get install -y graphviz"
      missing=true
    fi
  fi

  if [[ "$missing" == "true" ]]; then
    exit 1
  fi

  if [[ "$VERBOSE" == "true" ]]; then
    case "$METHOD" in
      cflow)
        print_status "cflow version: $(cflow --version 2>&1 | head -1)"
        ;;
      doxygen)
        print_status "doxygen version: $(doxygen --version 2>&1)"
        ;;
    esac
    if command -v dot &>/dev/null; then
      print_status "Graphviz version: $(dot -V 2>&1)"
    fi
  fi
}

# =============================================================================
# Source File Discovery
# =============================================================================

build_include_flags() {
  local include_dirs=(
    "src/inc"
    "src/shared"
    "src/shared/inc"
    "src/tasks/inc"
    "src/boot/inc"
    "src/boot/inc/custom"
    "src/boot/inc/smc_generated"
    "src/smc_gen"
    "src/smc_gen/r_bsp"
    "src/smc_gen/r_config"
    "src/smc_gen/general"
    "libs/threadx"
    "libs/threadx/common/inc"
    "libs/threadx/ports/rxv3/gnu/inc"
    "libs/rx_nanopb/nanopb"
    "libs/rx_nanopb/inc"
    "libs/rx_nanopb/inc/gen"
  )

  # Add all library module inc/ directories
  local lib_modules
  lib_modules=$(find "$PROJECT_ROOT/libs" -maxdepth 1 -mindepth 1 -type d \
    -not -name "threadx" -not -name "rx_nanopb" | sort)

  for mod_dir in $lib_modules; do
    local mod_name
    mod_name=$(basename "$mod_dir")
    if [[ -d "$mod_dir/inc" ]]; then
      include_dirs+=("libs/$mod_name/inc")
    fi
  done

  local flags=""
  for dir in "${include_dirs[@]}"; do
    if [[ -d "$PROJECT_ROOT/$dir" ]]; then
      flags="$flags -I$PROJECT_ROOT/$dir"
    fi
  done

  echo "$flags"
}

find_user_sources() {
  local scope="$1"
  local filter="$2"

  local exclude_args=()
  for excl in "${EXCLUDE_DIRS[@]}"; do
    exclude_args+=(-not -path "*/$excl/*")
  done

  case "$scope" in
    full)
      find "$PROJECT_ROOT/src" "$PROJECT_ROOT/libs" \
        -name "*.c" -type f \
        "${exclude_args[@]}" \
        2>/dev/null | sort
      ;;
    module)
      # Find source files for a specific module
      local mod_dir="$PROJECT_ROOT/libs/$filter"
      if [[ -d "$mod_dir" ]]; then
        find "$mod_dir" -name "*.c" -type f \
          "${exclude_args[@]}" \
          2>/dev/null | sort
      else
        print_error "Module directory not found: $mod_dir"
        exit 1
      fi
      ;;
    subsystem)
      local modules="${SUBSYSTEMS[$filter]}"
      if [[ -z "$modules" ]]; then
        print_error "Unknown subsystem: $filter"
        print_error "Available: ${!SUBSYSTEMS[*]}"
        exit 1
      fi
      for mod in $modules; do
        local mod_dir="$PROJECT_ROOT/libs/$mod"
        if [[ -d "$mod_dir" ]]; then
          find "$mod_dir" -name "*.c" -type f \
            "${exclude_args[@]}" \
            2>/dev/null
        else
          print_warning "Subsystem module directory not found: $mod_dir"
        fi
      done | sort
      ;;
    task)
      # Include the task file plus all library sources it may call
      local task_file="$PROJECT_ROOT/src/tasks/${filter}_task.c"
      if [[ ! -f "$task_file" ]]; then
        print_error "Task file not found: $task_file"
        exit 1
      fi
      echo "$task_file"
      # Also include all library sources for completeness
      find "$PROJECT_ROOT/libs" -name "*.c" -type f \
        "${exclude_args[@]}" \
        2>/dev/null | sort
      ;;
    function)
      # Include all sources (cflow will use --main to root at the function)
      find "$PROJECT_ROOT/src" "$PROJECT_ROOT/libs" \
        -name "*.c" -type f \
        "${exclude_args[@]}" \
        2>/dev/null | sort
      ;;
  esac
}

# =============================================================================
# cflow Method
# =============================================================================

build_external_exclude_pattern() {
  local pattern=""
  for prefix in "${EXTERNAL_PREFIXES[@]}"; do
    if [[ -n "$pattern" ]]; then
      pattern="$pattern|"
    fi
    pattern="$pattern^${prefix}"
  done
  echo "$pattern"
}

run_cflow() {
  print_step "Running cflow analysis (method=cflow, scope=$SCOPE)"

  local include_flags
  include_flags=$(build_include_flags)

  # Collect source files
  local sources
  case "$SCOPE" in
    full)     sources=$(find_user_sources "full" "") ;;
    module)   sources=$(find_user_sources "module" "$MODULE") ;;
    subsystem) sources=$(find_user_sources "subsystem" "$SUBSYSTEM") ;;
    task)     sources=$(find_user_sources "task" "$TASK") ;;
    function) sources=$(find_user_sources "function" "$FUNCTION") ;;
  esac

  local source_count
  source_count=$(echo "$sources" | wc -l)
  print_status "Found $source_count source files"

  if [[ "$VERBOSE" == "true" ]]; then
    echo "$sources" | while read -r f; do
      echo "  $f" >&2
    done
  fi

  # Build cflow command
  local cflow_args=(
    "--format=posix"
  )

  # Detect GCC C standard flag (c23 requires GCC 14+, older uses c2x)
  local c_std_flag="-std=c2x"
  if gcc -std=c23 -E -x c /dev/null &>/dev/null; then
    c_std_flag="-std=c23"
  fi

  # Try preprocessing to handle C23 typed enums and macros
  local preprocess_cmd="gcc -E $c_std_flag -D__RX__ -DTX_INCLUDE_USER_DEFINE_FILE -D__GNUC__ -D'__attribute__(x)=' -D'__asm__(x)=' $include_flags"

  # Test if preprocessing works on a sample file before committing to it
  local use_preprocess=false
  local sample_file
  sample_file=$(echo "$sources" | head -1)
  if [[ -n "$sample_file" ]] && eval "$preprocess_cmd" "$sample_file" &>/dev/null; then
    use_preprocess=true
    cflow_args+=("--preprocess=$preprocess_cmd")
    if [[ "$VERBOSE" == "true" ]]; then
      print_status "GCC preprocessing enabled ($c_std_flag)"
    fi
  else
    if [[ "$VERBOSE" == "true" ]]; then
      print_warning "GCC preprocessing unavailable (embedded headers not resolvable on host)"
      print_status "Using direct cflow parsing (works for most call analysis)"
    fi
  fi

  # Depth limit
  if [[ "$DEPTH" -gt 0 ]]; then
    cflow_args+=("--depth=$DEPTH")
  fi

  # Function-rooted graph
  if [[ "$SCOPE" == "function" && -n "$FUNCTION" ]]; then
    cflow_args+=("--main=$FUNCTION")
  fi

  if [[ "$SCOPE" == "task" && -n "$TASK" ]]; then
    cflow_args+=("--main=internal_${TASK}_task_entry")
  fi

  # Generate output filename
  local output_name
  case "$SCOPE" in
    full)      output_name="full_callgraph" ;;
    module)    output_name="module_${MODULE}" ;;
    subsystem) output_name="subsystem_${SUBSYSTEM}" ;;
    task)      output_name="task_${TASK}" ;;
    function)  output_name="function_${FUNCTION}" ;;
  esac

  local cflow_output="$OUTPUT_DIR/${output_name}.cflow"
  local dot_output="$OUTPUT_DIR/${output_name}.dot"
  local final_output="$OUTPUT_DIR/${output_name}.${FORMAT}"

  # Run cflow
  if [[ "$use_preprocess" == "true" ]]; then
    print_status "Running cflow with GCC preprocessing..."
  else
    print_status "Running cflow (direct source parsing)..."
  fi

  # shellcheck disable=SC2086
  echo "$sources" | xargs cflow "${cflow_args[@]}" > "$cflow_output" 2>/dev/null || {
    print_warning "cflow exited with warnings (common with embedded code)"
    # Retry without preprocessing if output is empty
    if [[ ! -s "$cflow_output" && "$use_preprocess" == "true" ]]; then
      print_warning "Retrying without preprocessing..."
      echo "$sources" | xargs cflow --format=posix > "$cflow_output" 2>/dev/null || true
    fi
  }

  if [[ ! -s "$cflow_output" ]]; then
    print_error "cflow produced no output"
    exit 1
  fi

  local node_count
  node_count=$(wc -l < "$cflow_output")
  print_status "cflow found $node_count call entries"

  # Convert to DOT format
  print_status "Converting cflow output to DOT format..."

  local cflow2dot_args=(
    "--title=${output_name//_/ }"
    "--rankdir=$RANKDIR"
  )

  # Build exclude pattern for external functions
  local exclude_pattern
  exclude_pattern=$(build_external_exclude_pattern)
  if [[ -n "$exclude_pattern" ]]; then
    cflow2dot_args+=("--exclude-pattern=$exclude_pattern")
  fi

  # Module filter for subsystem/module views
  if [[ "$SCOPE" == "module" && -n "$MODULE" ]]; then
    cflow2dot_args+=("--highlight-module=$MODULE")
  fi

  python3 "$SCRIPT_DIR/cflow2dot.py" "${cflow2dot_args[@]}" \
    < "$cflow_output" > "$dot_output"

  # Merge supplementary edges
  if [[ "$INCLUDE_THREADX_EDGES" == "true" && -f "$SCRIPT_DIR/threadx_connections.dot" ]]; then
    merge_supplementary_edges "$dot_output" "$SCRIPT_DIR/threadx_connections.dot"
  fi

  if [[ "$INCLUDE_ISR_EDGES" == "true" && -f "$SCRIPT_DIR/isr_connections.dot" ]]; then
    merge_supplementary_edges "$dot_output" "$SCRIPT_DIR/isr_connections.dot"
  fi

  # Apply scope filter if needed
  if [[ "$SCOPE" == "module" || "$SCOPE" == "subsystem" ]]; then
    local include_pattern
    case "$SCOPE" in
      module)
        include_pattern="^(${MODULE}_|internal_${MODULE}_|priv_${MODULE}_)"
        ;;
      subsystem)
        local modules="${SUBSYSTEMS[$SUBSYSTEM]}"
        local mod_pattern=""
        for mod in $modules; do
          local base="${mod#rx_}"
          if [[ -n "$mod_pattern" ]]; then
            mod_pattern="$mod_pattern|"
          fi
          mod_pattern="${mod_pattern}${mod}_|internal_${mod}_|priv_${mod}_|${base}_"
        done
        include_pattern="^($mod_pattern)"
        ;;
    esac

    if [[ -n "$include_pattern" ]]; then
      local filtered_dot="${dot_output%.dot}_filtered.dot"
      python3 "$SCRIPT_DIR/filter_dot.py" \
        --include-pattern="$include_pattern" \
        < "$dot_output" > "$filtered_dot"
      mv "$filtered_dot" "$dot_output"
    fi
  fi

  # Render graph
  if [[ "$FORMAT" == "dot" ]]; then
    final_output="$dot_output"
    print_success "DOT output: $final_output"
  else
    render_dot "$dot_output" "$final_output"
  fi

  # Clean up intermediate cflow file
  if [[ "$VERBOSE" != "true" ]]; then
    rm -f "$cflow_output"
  fi

  echo "$final_output"
}

# =============================================================================
# RTL Method
# =============================================================================

run_rtl() {
  print_step "Running RTL analysis (method=rtl)"

  local expand_files
  expand_files=$(find "$PROJECT_ROOT" -name "*.expand" -type f 2>/dev/null)

  if [[ -z "$expand_files" ]]; then
    print_error "No .expand RTL dump files found."
    echo "" >&2
    echo "To generate RTL dumps:" >&2
    echo "  1. In e2 studio: Project Properties -> C/C++ Build -> Settings" >&2
    echo "  2. Compiler -> Other -> Add '-fdump-rtl-expand' to flags" >&2
    echo "  3. Rebuild the project" >&2
    echo "  4. Re-run this script with --method=rtl" >&2
    echo "" >&2
    echo "Alternative (host GCC, logic only):" >&2
    echo "  gcc -fdump-rtl-expand -std=c23 -c src/*.c libs/*/src/*.c" >&2
    exit 1
  fi

  local expand_count
  expand_count=$(echo "$expand_files" | wc -l)
  print_status "Found $expand_count RTL dump files"

  local output_name="rtl_callgraph"
  local dot_output="$OUTPUT_DIR/${output_name}.dot"
  local final_output="$OUTPUT_DIR/${output_name}.${FORMAT}"

  if command -v egypt &>/dev/null; then
    print_status "Using egypt for RTL parsing..."
    # shellcheck disable=SC2086
    echo "$expand_files" | xargs egypt > "$dot_output" 2>/dev/null
  else
    print_status "Parsing RTL dumps manually..."
    parse_rtl_manual "$expand_files" > "$dot_output"
  fi

  if [[ "$FORMAT" == "dot" ]]; then
    final_output="$dot_output"
  else
    render_dot "$dot_output" "$final_output"
  fi

  print_success "RTL call graph: $final_output"
  echo "$final_output"
}

parse_rtl_manual() {
  local files="$1"

  echo "digraph rtl_callgraph {"
  echo "  rankdir=$RANKDIR;"
  echo '  node [shape=box, style=filled, fillcolor="#E8E8E8", fontname="Helvetica", fontsize=10];'
  echo '  edge [color="#666666"];'

  # Parse RTL dumps for call instructions
  # Pattern: (call_insn ... (symbol_ref:SI ("function_name"))
  echo "$files" | while read -r f; do
    local caller
    caller=$(basename "$f" .c.expand)
    # Extract called function symbols from RTL
    grep -oP 'symbol_ref:SI \("([^"]+)"\)' "$f" 2>/dev/null | \
      sed 's/symbol_ref:SI ("//;s/")//' | \
      sort -u | while read -r callee; do
        echo "  \"$caller\" -> \"$callee\";"
      done
  done

  echo "}"
}

# =============================================================================
# Doxygen Method
# =============================================================================

run_doxygen() {
  print_step "Running Doxygen analysis (method=doxygen)"

  local doxyfile="$OUTPUT_DIR/Doxyfile.callgraph"
  local doxy_output="$OUTPUT_DIR/doxygen_html"

  local include_flags
  include_flags=$(build_include_flags)
  # Convert -I flags to space-separated paths for Doxygen
  local include_paths
  include_paths=$(echo "$include_flags" | sed 's/-I//g')

  # Generate Doxyfile
  print_status "Generating Doxyfile for call graph analysis..."
  cat > "$doxyfile" <<DOXYEOF
# Doxyfile for STAR RX72N Call Graph Generation
# Auto-generated by generate_callgraph.sh

PROJECT_NAME           = "STAR RX72N Firmware"
PROJECT_BRIEF          = "Call Graph Analysis"
OUTPUT_DIRECTORY       = $doxy_output

# Input sources
INPUT                  = $PROJECT_ROOT/src $PROJECT_ROOT/libs
FILE_PATTERNS          = *.c *.h
RECURSIVE              = YES
EXCLUDE_PATTERNS       = */threadx/* */nanopb/* */smc_gen/* */smc_generated/* */tests/*

# Include paths
INCLUDE_PATH           = $include_paths

# Preprocessing
ENABLE_PREPROCESSING   = YES
MACRO_EXPANSION        = YES
PREDEFINED             = __RX__ TX_INCLUDE_USER_DEFINE_FILE __attribute__(x)= __asm__(x)=

# Call graphs
HAVE_DOT               = YES
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
DOT_IMAGE_FORMAT       = svg
INTERACTIVE_SVG        = YES
DOT_GRAPH_MAX_NODES    = 200
MAX_DOT_GRAPH_DEPTH    = 0

# Output settings
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
HTML_DYNAMIC_SECTIONS  = YES
DISABLE_INDEX          = NO
GENERATE_TREEVIEW      = YES

# Source browsing
SOURCE_BROWSER         = YES
INLINE_SOURCES         = NO
REFERENCED_BY_RELATION = YES
REFERENCES_RELATION    = YES

# Extraction
EXTRACT_ALL            = YES
EXTRACT_STATIC         = YES
EXTRACT_PRIVATE        = YES
EXTRACT_LOCAL_CLASSES  = YES

# Warnings
QUIET                  = YES
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = NO
DOXYEOF

  # Run Doxygen
  print_status "Running Doxygen (this may take a minute)..."
  doxygen "$doxyfile" 2>/dev/null

  local index_file="$doxy_output/html/index.html"
  if [[ -f "$index_file" ]]; then
    print_success "Doxygen HTML docs: $index_file"
    echo "$index_file"
  else
    print_error "Doxygen failed to generate output"
    exit 1
  fi
}

# =============================================================================
# Graph Utilities
# =============================================================================

merge_supplementary_edges() {
  local main_dot="$1"
  local supplement_dot="$2"

  if [[ "$VERBOSE" == "true" ]]; then
    print_status "Merging supplementary edges from $(basename "$supplement_dot")"
  fi

  # Insert supplementary edges before the closing brace of the main DOT file
  local temp_file
  temp_file=$(mktemp)

  # Extract edge lines from supplement (skip comments and graph wrapper)
  local supplement_edges
  supplement_edges=$(grep -E '^\s*"[^"]+" -> "[^"]+"' "$supplement_dot" 2>/dev/null || true)

  if [[ -n "$supplement_edges" ]]; then
    # Insert edges before the last line (closing brace)
    head -n -1 "$main_dot" > "$temp_file"
    echo "" >> "$temp_file"
    echo "  /* Supplementary edges from $(basename "$supplement_dot") */" >> "$temp_file"
    echo "$supplement_edges" >> "$temp_file"
    echo "}" >> "$temp_file"
    mv "$temp_file" "$main_dot"
  else
    rm -f "$temp_file"
  fi
}

render_dot() {
  local dot_file="$1"
  local output_file="$2"
  local fmt="${output_file##*.}"

  print_status "Rendering graph with $LAYOUT layout -> $output_file"

  local dot_args=("-T$fmt")

  # Use sfdp for large graphs
  local node_count
  node_count=$(grep -c '^\s*"[^"]*"\s*\[' "$dot_file" 2>/dev/null || echo "0")

  local renderer="$LAYOUT"
  if [[ "$node_count" -gt 500 && "$LAYOUT" == "dot" ]]; then
    print_warning "Large graph ($node_count nodes) - consider --layout=sfdp for better rendering"
  fi

  "$renderer" "${dot_args[@]}" -o "$output_file" "$dot_file" 2>/dev/null || {
    print_error "Graph rendering failed (${renderer})"
    print_warning "Try: --format=dot to get raw DOT output, or --layout=sfdp for large graphs"
    exit 1
  }

  local size
  size=$(du -h "$output_file" 2>/dev/null | cut -f1)
  print_success "Generated: $output_file ($size)"
}

# =============================================================================
# Main Execution
# =============================================================================

main() {
  parse_args "$@"

  # Validate arguments
  if [[ "$SCOPE" == "module" && -z "$MODULE" ]]; then
    print_error "Module name required: --module=NAME"
    exit 1
  fi
  if [[ "$SCOPE" == "subsystem" && -z "$SUBSYSTEM" ]]; then
    print_error "Subsystem name required: --subsystem=NAME"
    exit 1
  fi
  if [[ "$SCOPE" == "task" && -z "$TASK" ]]; then
    print_error "Task name required: --task=NAME"
    exit 1
  fi
  if [[ "$SCOPE" == "function" && -z "$FUNCTION" ]]; then
    print_error "Function name required: --function=NAME"
    exit 1
  fi

  # Check dependencies
  check_dependencies

  # Create output directory
  mkdir -p "$OUTPUT_DIR"

  print_status "Call graph generation: method=$METHOD, scope=$SCOPE, format=$FORMAT"

  # Run selected method
  case "$METHOD" in
    cflow)  run_cflow ;;
    rtl)    run_rtl ;;
    doxygen) run_doxygen ;;
  esac
}

main "$@"
