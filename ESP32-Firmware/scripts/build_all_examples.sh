#!/bin/bash

# Script to build all examples and log results
# Creates individual logs and a summary overview

# Determine project root directory (parent of scripts directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to project root
cd "$PROJECT_ROOT" || exit 1

# Color codes for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create logs directory
LOGS_DIR="build_logs"
mkdir -p "$LOGS_DIR"

# Get current timestamp
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Overview file
OVERVIEW_FILE="$LOGS_DIR/build_overview_$TIMESTAMP.txt"

# Initialize counters
TOTAL_EXAMPLES=0
SUCCESS_COUNT=0
FAILED_COUNT=0

# Check if platformio.ini exists
if [ ! -f "platformio.ini" ]; then
    echo -e "${RED}ERROR: platformio.ini not found in $PROJECT_ROOT${NC}"
    echo "Make sure you're running this script from the project root or scripts directory"
    exit 1
fi

# Extract all example environment names from platformio.ini
EXAMPLES=$(grep -E '^\[env:example_[0-9]+' platformio.ini | sed 's/\[env:\(.*\)\]/\1/')

echo "=================================================" | tee "$OVERVIEW_FILE"
echo "ESP32 Firmware - Example Build Report" | tee -a "$OVERVIEW_FILE"
echo "Build Date: $(date)" | tee -a "$OVERVIEW_FILE"
echo "Project Root: $PROJECT_ROOT" | tee -a "$OVERVIEW_FILE"
echo "=================================================" | tee -a "$OVERVIEW_FILE"
echo "" | tee -a "$OVERVIEW_FILE"

# Build each example
for EXAMPLE in $EXAMPLES; do
    TOTAL_EXAMPLES=$((TOTAL_EXAMPLES + 1))

    # Extract example number for display
    EXAMPLE_NUM=$(echo "$EXAMPLE" | sed 's/example_//')

    echo -e "${YELLOW}Building $EXAMPLE ($TOTAL_EXAMPLES)...${NC}"

    # Create log file for this example
    LOG_FILE="$LOGS_DIR/${EXAMPLE}_$TIMESTAMP.log"

    # Build the example and capture output
    pio run -e "$EXAMPLE" > "$LOG_FILE" 2>&1
    BUILD_STATUS=$?

    # Check if build succeeded
    if [ $BUILD_STATUS -eq 0 ]; then
        echo -e "${GREEN}✓ SUCCESS${NC} - $EXAMPLE"
        echo "✓ SUCCESS - $EXAMPLE" >> "$OVERVIEW_FILE"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo -e "${RED}✗ FAILED${NC}  - $EXAMPLE"
        echo "✗ FAILED  - $EXAMPLE" >> "$OVERVIEW_FILE"

        # Extract error summary from log
        echo "  Error details:" >> "$OVERVIEW_FILE"
        grep -i "error:" "$LOG_FILE" | head -5 | sed 's/^/    /' >> "$OVERVIEW_FILE"

        FAILED_COUNT=$((FAILED_COUNT + 1))
    fi

    echo "" # Blank line for readability
done

# Write summary
echo "" | tee -a "$OVERVIEW_FILE"
echo "=================================================" | tee -a "$OVERVIEW_FILE"
echo "Build Summary" | tee -a "$OVERVIEW_FILE"
echo "=================================================" | tee -a "$OVERVIEW_FILE"
echo "Total Examples: $TOTAL_EXAMPLES" | tee -a "$OVERVIEW_FILE"
echo -e "${GREEN}Successful: $SUCCESS_COUNT${NC}" | tee -a "$OVERVIEW_FILE"
echo "Successful: $SUCCESS_COUNT" >> "$OVERVIEW_FILE"
echo -e "${RED}Failed: $FAILED_COUNT${NC}" | tee -a "$OVERVIEW_FILE"
echo "Failed: $FAILED_COUNT" >> "$OVERVIEW_FILE"

if [ $FAILED_COUNT -eq 0 ]; then
    echo -e "\n${GREEN}All examples built successfully!${NC}" | tee -a "$OVERVIEW_FILE"
else
    echo -e "\n${YELLOW}Some examples failed to build. Check individual logs in $LOGS_DIR${NC}" | tee -a "$OVERVIEW_FILE"
fi

echo "" | tee -a "$OVERVIEW_FILE"
echo "Individual build logs saved in: $LOGS_DIR" | tee -a "$OVERVIEW_FILE"
echo "Overview saved to: $OVERVIEW_FILE" | tee -a "$OVERVIEW_FILE"
