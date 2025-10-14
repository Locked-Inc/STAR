#!/bin/bash
# Docker-based build script for STAR-Z2
# Provides reproducible build environment

set -e

# Color output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Building STAR-Z2 in Docker container${NC}"

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/../.."

# Build Docker image
echo -e "${YELLOW}Building Docker image...${NC}"
docker build -t star-pynq-builder "$SCRIPT_DIR"

# Run build in container
echo -e "${YELLOW}Starting build in container...${NC}"
docker run --rm -it \
    -v "$PROJECT_ROOT:/workspace" \
    -e PREBUILT_IMAGE="${PREBUILT_IMAGE:-}" \
    star-pynq-builder \
    bash -c "cd /workspace && ./scripts/build.sh ${1:-prebuilt}"

echo -e "${GREEN}Build complete!${NC}"
