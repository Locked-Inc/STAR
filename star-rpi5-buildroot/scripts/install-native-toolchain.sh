#!/bin/bash
# Install native GCC toolchain on target
# This script copies the cross-compiler as a native compiler to the target
# Since both host cross-compiler and target are aarch64, the binaries work natively

set -e

TARGET_DIR="$1"
HOST_DIR="$2"

if [ -z "$TARGET_DIR" ] || [ -z "$HOST_DIR" ]; then
    echo "Usage: $0 <target_dir> <host_dir>"
    exit 1
fi

echo "Installing native GCC toolchain to target..."

# Create target directories
mkdir -p "$TARGET_DIR/usr/bin"
mkdir -p "$TARGET_DIR/usr/lib/gcc"
mkdir -p "$TARGET_DIR/usr/include"
mkdir -p "$TARGET_DIR/usr/libexec/gcc"

# Find the cross-compiler prefix (e.g., aarch64-buildroot-linux-gnu)
CROSS_PREFIX=$(ls "$HOST_DIR/bin/"*-gcc 2>/dev/null | head -1 | xargs basename | sed 's/-gcc$//')

if [ -z "$CROSS_PREFIX" ]; then
    echo "Warning: Could not find cross-compiler in $HOST_DIR/bin"
    echo "Native GCC will not be installed"
    exit 0
fi

echo "Found cross-compiler prefix: $CROSS_PREFIX"

# Copy GCC binaries as native compilers
# The cross-compiler for aarch64 can run natively on aarch64 target
# Note: Remove any existing symlinks first to avoid overwriting busybox
for tool in gcc g++ cpp gcov gcc-ar gcc-nm gcc-ranlib; do
    if [ -f "$HOST_DIR/bin/${CROSS_PREFIX}-${tool}" ]; then
        # Remove any existing file/symlink first to avoid following symlinks
        rm -f "$TARGET_DIR/usr/bin/${tool}"
        cp "$HOST_DIR/bin/${CROSS_PREFIX}-${tool}" "$TARGET_DIR/usr/bin/${tool}"
        chmod +x "$TARGET_DIR/usr/bin/${tool}"
        echo "  Installed: ${tool}"
    fi
done

# Create cc symlink
if [ -f "$TARGET_DIR/usr/bin/gcc" ]; then
    ln -sf gcc "$TARGET_DIR/usr/bin/cc"
    echo "  Created symlink: cc -> gcc"
fi

# Create c++ symlink
if [ -f "$TARGET_DIR/usr/bin/g++" ]; then
    ln -sf g++ "$TARGET_DIR/usr/bin/c++"
    echo "  Created symlink: c++ -> g++"
fi

# Copy binutils
# Note: Remove any existing symlinks first to avoid overwriting busybox
# (busybox creates symlinks like /usr/bin/strings -> busybox)
for tool in ar as ld ld.bfd nm objcopy objdump ranlib readelf size strings strip; do
    if [ -f "$HOST_DIR/bin/${CROSS_PREFIX}-${tool}" ]; then
        # Remove any existing file/symlink first to avoid following symlinks
        rm -f "$TARGET_DIR/usr/bin/${tool}"
        cp "$HOST_DIR/bin/${CROSS_PREFIX}-${tool}" "$TARGET_DIR/usr/bin/${tool}"
        chmod +x "$TARGET_DIR/usr/bin/${tool}"
        echo "  Installed: ${tool}"
    fi
done

# Copy GCC internal libraries and specs
GCC_VERSION=$(ls "$HOST_DIR/lib/gcc/${CROSS_PREFIX}/" 2>/dev/null | head -1)
if [ -n "$GCC_VERSION" ]; then
    echo "Found GCC version: $GCC_VERSION"

    # Copy GCC lib directory structure
    mkdir -p "$TARGET_DIR/usr/lib/gcc/${CROSS_PREFIX}/${GCC_VERSION}"
    if [ -d "$HOST_DIR/lib/gcc/${CROSS_PREFIX}/${GCC_VERSION}" ]; then
        cp -a "$HOST_DIR/lib/gcc/${CROSS_PREFIX}/${GCC_VERSION}"/* \
              "$TARGET_DIR/usr/lib/gcc/${CROSS_PREFIX}/${GCC_VERSION}/" 2>/dev/null || true
        echo "  Installed GCC libraries"
    fi

    # Copy libexec (cc1, cc1plus, collect2, lto-wrapper)
    mkdir -p "$TARGET_DIR/usr/libexec/gcc/${CROSS_PREFIX}/${GCC_VERSION}"
    if [ -d "$HOST_DIR/libexec/gcc/${CROSS_PREFIX}/${GCC_VERSION}" ]; then
        cp -a "$HOST_DIR/libexec/gcc/${CROSS_PREFIX}/${GCC_VERSION}"/* \
              "$TARGET_DIR/usr/libexec/gcc/${CROSS_PREFIX}/${GCC_VERSION}/" 2>/dev/null || true
        echo "  Installed GCC executables (cc1, cc1plus, etc.)"
    fi
fi

# Copy C library headers (already in target from glibc, but ensure they exist)
if [ ! -d "$TARGET_DIR/usr/include/linux" ]; then
    echo "Warning: Linux headers not found in target"
fi

# Copy C++ headers
if [ -d "$HOST_DIR/${CROSS_PREFIX}/include/c++" ]; then
    mkdir -p "$TARGET_DIR/usr/include/c++"
    cp -a "$HOST_DIR/${CROSS_PREFIX}/include/c++"/* "$TARGET_DIR/usr/include/c++/" 2>/dev/null || true
    echo "  Installed C++ headers"
fi

echo "Native GCC toolchain installation complete!"
echo ""
echo "Installed tools:"
ls -la "$TARGET_DIR/usr/bin/gcc" "$TARGET_DIR/usr/bin/g++" "$TARGET_DIR/usr/bin/cc" 2>/dev/null || true
