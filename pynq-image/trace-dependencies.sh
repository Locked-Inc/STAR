#!/bin/bash

# Script to trace which packages in multistrap.config pull unwanted dependencies
# Uses apt-cache to check ARM architecture packages

echo "=== Tracing Dependency Chains ==="
echo ""

# Unwanted packages we need to trace
UNWANTED=(
    "perl"
    "perl-base"
    "libperl5.34"
    "xkb-data"
    "libxau6"
    "libxcb1"
    "libxdmcp6"
    "console-setup"
    "kbd"
    "keyboard-configuration"
    "fonts-dejavu-core"
    "fontconfig-config"
    "libasound2"
    "libcups2"
    "debconf-i18n"
    "lsb-release"
    "sensible-utils"
    "ucf"
    "mailcap"
    "mime-support"
)

# Main suspects from multistrap.config
SUSPECTS=(
    "openjdk-17-jre-headless"
    "python3-pil"
    "git"
    "vim-tiny"
    "ubuntu-minimal"
    "python3-numpy"
    "libatlas3-base"
)

echo "Checking which packages pull unwanted dependencies..."
echo "=================================================="

for suspect in "${SUSPECTS[@]}"; do
    echo ""
    echo "### Checking: $suspect"

    # Get dependencies
    deps=$(apt-cache depends "$suspect" 2>/dev/null | grep "Depends:" | awk '{print $2}')

    # Check if any unwanted package is in dependencies
    for unwanted in "${UNWANTED[@]}"; do
        if echo "$deps" | grep -q "^${unwanted}$"; then
            echo "  ⚠️  PULLS: $unwanted"
        fi
    done

    # Also check Recommends (should be off but let's verify)
    recommends=$(apt-cache depends "$suspect" 2>/dev/null | grep "Recommends:" | awk '{print $2}')
    for unwanted in "${UNWANTED[@]}"; do
        if echo "$recommends" | grep -q "^${unwanted}$"; then
            echo "  ℹ️  RECOMMENDS: $unwanted (should be ignored with allowrecommends=false)"
        fi
    done
done

echo ""
echo "=================================================="
echo "Reverse dependency check (which packages require these bloat packages):"
echo ""

for unwanted in "${UNWANTED[@]}"; do
    echo "### $unwanted is required by:"
    apt-cache rdepends "$unwanted" 2>/dev/null | grep -v "^$unwanted" | grep -v "Reverse Depends:" | head -5
    echo ""
done
