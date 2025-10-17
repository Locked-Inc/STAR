#!/bin/bash

echo "=== Deep Dependency Chain Trace ==="
echo ""

# Check what libcups2 pulls in
echo "### libcups2 dependencies (pulled by openjdk-17-jre-headless):"
apt-cache depends libcups2 | grep "Depends:" | awk '{print "  " $2}'
echo ""

# Check what libasound2 pulls in
echo "### libasound2 dependencies (pulled by openjdk-17-jre-headless):"
apt-cache depends libasound2 | grep "Depends:" | awk '{print "  " $2}'
echo ""

# Check what fontconfig-config pulls in
echo "### fontconfig-config reverse deps:"
apt-cache rdepends fontconfig-config | grep -v "^fontconfig-config" | grep -v "Reverse Depends:" | head -10
echo ""

# Check libfontconfig1
echo "### libfontconfig1 dependencies:"
apt-cache depends libfontconfig1 | grep "Depends:" | awk '{print "  " $2}'
echo ""

# Check which package pulls libfontconfig1
echo "### libfontconfig1 reverse deps:"
apt-cache rdepends libfontconfig1 | grep -v "^libfontconfig1" | grep -v "Reverse Depends:" | head -10
echo ""

# Check which package in our list needs libxcb1
echo "### Checking which suspect packages need X11 libraries:"
for pkg in openjdk-17-jre-headless python3-pil git vim-tiny ubuntu-minimal python3-numpy libatlas3-base libcups2; do
    if apt-cache depends "$pkg" 2>/dev/null | grep -q "libxcb\|libxau\|libxdmcp"; then
        echo "  ⚠️  $pkg needs X11 libraries"
        apt-cache depends "$pkg" 2>/dev/null | grep -E "libxcb|libxau|libxdmcp" | awk '{print "      " $0}'
    fi
done
echo ""

# Check what libgfortran5 pulls (might be from libatlas3-base)
echo "### libgfortran5 reverse deps (appears in install list):"
apt-cache rdepends libgfortran5 | grep -v "^libgfortran5" | grep -v "Reverse Depends:" | head -10
