#!/bin/bash

# Script to build all examples in parallel and log results
# Creates individual logs and a summary overview

# Number of parallel jobs (adjust based on CPU cores)
MAX_PARALLEL=${1:-8}

# Determine project root directory (parent of scripts directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to project root
cd "$PROJECT_ROOT" || exit 1

# Color codes for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Create logs directory
LOGS_DIR="build_logs"
mkdir -p "$LOGS_DIR"

# Get current timestamp
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Overview file
OVERVIEW_FILE="$LOGS_DIR/build_overview_$TIMESTAMP.txt"

# Temporary files for tracking results
TEMP_SUCCESS="$LOGS_DIR/.success_$TIMESTAMP.tmp"
TEMP_FAILED="$LOGS_DIR/.failed_$TIMESTAMP.tmp"
TEMP_ERRORS="$LOGS_DIR/.errors_$TIMESTAMP.tmp"
touch "$TEMP_SUCCESS" "$TEMP_FAILED" "$TEMP_ERRORS"

# Trap to clean up temp files on exit
trap "rm -f $TEMP_SUCCESS $TEMP_FAILED $TEMP_ERRORS" EXIT

echo -e "${CYAN}=================================================${NC}" | tee "$OVERVIEW_FILE"
echo -e "${CYAN}ESP32 Firmware - Parallel Example Build Report${NC}" | tee -a "$OVERVIEW_FILE"
echo "Build Date: $(date)" | tee -a "$OVERVIEW_FILE"
echo "Max Parallel Builds: $MAX_PARALLEL" | tee -a "$OVERVIEW_FILE"
echo "Project Root: $PROJECT_ROOT" | tee -a "$OVERVIEW_FILE"
echo -e "${CYAN}=================================================${NC}" | tee -a "$OVERVIEW_FILE"
echo "" | tee -a "$OVERVIEW_FILE"

# Check if platformio.ini exists
if [ ! -f "platformio.ini" ]; then
    echo -e "${RED}ERROR: platformio.ini not found in $PROJECT_ROOT${NC}"
    echo "Make sure you're running this script from the project root or scripts directory"
    exit 1
fi

# Extract all example environment names from platformio.ini
EXAMPLES=$(grep -E '^\[env:example_[0-9]+' platformio.ini | sed 's/\[env:\(.*\)\]/\1/')
TOTAL_EXAMPLES=$(echo "$EXAMPLES" | wc -l)

echo -e "${YELLOW}Found $TOTAL_EXAMPLES examples to build${NC}"
echo ""

# Function to build a single example
build_example() {
    local EXAMPLE=$1
    local LOG_FILE="$LOGS_DIR/${EXAMPLE}_$TIMESTAMP.log"

    echo -e "${YELLOW}Building $EXAMPLE...${NC}"

    # Build the example and capture output
    if pio run -e "$EXAMPLE" > "$LOG_FILE" 2>&1; then
        echo -e "${GREEN}✓ SUCCESS${NC} - $EXAMPLE"
        echo "$EXAMPLE" >> "$TEMP_SUCCESS"
        echo "✓ SUCCESS - $EXAMPLE" >> "$TEMP_ERRORS"
    else
        echo -e "${RED}✗ FAILED${NC}  - $EXAMPLE"
        echo "$EXAMPLE" >> "$TEMP_FAILED"
        echo "✗ FAILED  - $EXAMPLE" >> "$TEMP_ERRORS"

        # Extract error summary
        echo "  Error details:" >> "$TEMP_ERRORS"
        grep -i "error:" "$LOG_FILE" | head -5 | sed 's/^/    /' >> "$TEMP_ERRORS"
    fi
}

# Export function and variables for parallel execution
export -f build_example
export LOGS_DIR TIMESTAMP TEMP_SUCCESS TEMP_FAILED TEMP_ERRORS RED GREEN YELLOW NC

# Start timer
START_TIME=$(date +%s)

# Build all examples in parallel using GNU parallel or xargs
if command -v parallel &> /dev/null; then
    # Use GNU parallel if available (best option)
    echo "$EXAMPLES" | parallel -j "$MAX_PARALLEL" build_example {}
elif command -v xargs &> /dev/null; then
    # Fallback to xargs with -P flag
    echo "$EXAMPLES" | xargs -I {} -P "$MAX_PARALLEL" bash -c 'build_example "$@"' _ {}
else
    # Fallback to sequential if no parallel tool available
    echo -e "${YELLOW}Warning: Neither 'parallel' nor 'xargs' found. Building sequentially...${NC}"
    for EXAMPLE in $EXAMPLES; do
        build_example "$EXAMPLE"
    done
fi

# Calculate duration
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
HOURS=$((DURATION / 3600))
MINUTES=$(((DURATION % 3600) / 60))
SECONDS=$((DURATION % 60))
DURATION_STR=$(printf "%02d:%02d:%02d" $HOURS $MINUTES $SECONDS)

# Count results
SUCCESS_COUNT=$(wc -l < "$TEMP_SUCCESS" | tr -d ' ')
FAILED_COUNT=$(wc -l < "$TEMP_FAILED" | tr -d ' ')

# Sort and write all results to overview file
echo "" >> "$OVERVIEW_FILE"
echo "=================================================" >> "$OVERVIEW_FILE"
echo "Build Results" >> "$OVERVIEW_FILE"
echo "=================================================" >> "$OVERVIEW_FILE"
cat "$TEMP_ERRORS" >> "$OVERVIEW_FILE"

# Write summary
echo "" | tee -a "$OVERVIEW_FILE"
echo "=================================================" | tee -a "$OVERVIEW_FILE"
echo "Build Summary" | tee -a "$OVERVIEW_FILE"
echo "=================================================" | tee -a "$OVERVIEW_FILE"
echo "Total Examples: $TOTAL_EXAMPLES" | tee -a "$OVERVIEW_FILE"
echo "Successful: $SUCCESS_COUNT" >> "$OVERVIEW_FILE"
echo "Failed: $FAILED_COUNT" >> "$OVERVIEW_FILE"
echo "Build Duration: $DURATION_STR" | tee -a "$OVERVIEW_FILE"

# Display colored summary
echo -e "${GREEN}Successful: $SUCCESS_COUNT${NC}"
echo -e "${RED}Failed: $FAILED_COUNT${NC}"

if [ "$FAILED_COUNT" -eq 0 ]; then
    echo -e "\n${GREEN}All examples built successfully!${NC}" | tee -a "$OVERVIEW_FILE"
else
    echo -e "\n${YELLOW}Some examples failed to build. Check individual logs in $LOGS_DIR${NC}" | tee -a "$OVERVIEW_FILE"
fi

echo "" | tee -a "$OVERVIEW_FILE"
echo "Individual build logs saved in: $LOGS_DIR" | tee -a "$OVERVIEW_FILE"
echo "Overview saved to: $OVERVIEW_FILE" | tee -a "$OVERVIEW_FILE"
