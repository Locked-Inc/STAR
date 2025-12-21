#!/bin/bash
# Clean all build artifacts

set -e

echo "Cleaning build artifacts..."

# Remove build directory
if [ -d "build" ]; then
    rm -rf build
    echo "✓ Removed build/"
else
    echo "  build/ already clean"
fi

# Remove any CMake cache files that might be outside build/
find . -name "CMakeCache.txt" -not -path "./build/*" -delete 2>/dev/null || true
find . -name "CMakeFiles" -type d -not -path "./build/*" -exec rm -rf {} + 2>/dev/null || true

echo ""
echo "Clean complete! Run ./build.sh to rebuild."
