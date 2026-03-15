#!/bin/bash
# verify-nanopb.sh - Verify nanopb installation and buf generate
# STAR Project - Protocol Buffer verification script

set -e

echo "========================================="
echo "STAR Protocol Buffer Verification"
echo "========================================="
echo ""

# Check if protoc-gen-nanopb is installed
echo "1. Checking protoc-gen-nanopb installation..."
if ! command -v protoc-gen-nanopb &> /dev/null; then
    echo "   [FAIL] ERROR: protoc-gen-nanopb not found in PATH"
    exit 1
fi

NANOPB_VERSION=$(python3 -m pip show nanopb 2>/dev/null | grep "^Version:" | cut -d' ' -f2 || echo "unknown")
echo "   [PASS] protoc-gen-nanopb found (nanopb version: $NANOPB_VERSION)"
echo ""

# Check if buf is installed
echo "2. Checking buf installation..."
if ! command -v buf &> /dev/null; then
    echo "   [FAIL] ERROR: buf not found in PATH"
    exit 1
fi

BUF_VERSION=$(buf --version)
echo "   [PASS] buf found: $BUF_VERSION"
echo ""

# Verify buf configuration
echo "3. Verifying buf configuration..."
cd /workspaces/STAR/star-proto/proto || exit 1

if buf lint; then
    echo "   [PASS] buf lint passed"
else
    echo "   [WARN] buf lint warnings (non-fatal)"
fi
echo ""

# Update dependencies
echo "4. Updating buf dependencies..."
if buf mod update; then
    echo "   [PASS] buf dependencies updated"
else
    echo "   [FAIL] ERROR: Failed to update buf dependencies"
    exit 1
fi
echo ""

# Generate code
echo "5. Running buf generate..."
cd /workspaces/STAR/star-proto || exit 1

if buf generate proto; then
    echo "   [PASS] buf generate completed successfully"
else
    echo "   [FAIL] ERROR: buf generate failed"
    exit 1
fi
echo ""

# Verify generated files
echo "6. Verifying generated nanopb files..."
NANOPB_DIR="/workspaces/STAR/star-proto/gen/nanopb/star/v1"

if [ ! -d "$NANOPB_DIR" ]; then
    echo "   [FAIL] ERROR: nanopb output directory not found"
    exit 1
fi

NANOPB_FILES=$(find "$NANOPB_DIR" -name "*.pb.h" -o -name "*.pb.c" | wc -l)
if [ "$NANOPB_FILES" -eq 0 ]; then
    echo "   [FAIL] ERROR: No nanopb files generated"
    exit 1
fi

echo "   [PASS] Found $NANOPB_FILES nanopb files in $NANOPB_DIR"
echo ""

# List generated files
echo "Generated nanopb files:"
find "$NANOPB_DIR" -name "*.pb.h" -o -name "*.pb.c" | sed 's/^/   /'
echo ""

echo "========================================="
echo "[PASS] All verification checks passed!"
echo "========================================="
echo ""
echo "Next steps:"
echo "  1. Generated code is ready for use in star-rx72n-firmware/"
echo "  2. Commit buf.lock if it was created/updated"
echo "  3. Run 'buf generate proto' anytime you modify .proto files"
