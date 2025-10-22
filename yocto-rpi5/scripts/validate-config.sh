#!/bin/bash
#
# Validate Yocto configuration files for STAR Raspberry Pi 5 image
# Checks syntax and references without requiring full Yocto setup
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YOCTO_ROOT="$(dirname "${SCRIPT_DIR}")"

echo "========================================="
echo "STAR Yocto Configuration Validation"
echo "========================================="
echo ""

cd "${YOCTO_ROOT}"

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

ERRORS=0
WARNINGS=0

# Function to print success
success() {
    echo -e "${GREEN}✓${NC} $1"
}

# Function to print error
error() {
    echo -e "${RED}✗${NC} $1"
    ((ERRORS++))
}

# Function to print warning
warning() {
    echo -e "${YELLOW}⚠${NC} $1"
    ((WARNINGS++))
}

echo "Checking configuration files..."
echo ""

# 1. Check .gitmodules
echo "1. Checking .gitmodules..."
if [ -f ".gitmodules" ]; then
    if grep -q "meta-openjdk-temurin" .gitmodules; then
        success "meta-openjdk-temurin layer configured in .gitmodules"
    else
        error "meta-openjdk-temurin layer missing from .gitmodules"
    fi

    if grep -q "scarthgap" .gitmodules; then
        success "Using Yocto Scarthgap (5.0) branch"
    else
        warning "Yocto branch not set to scarthgap"
    fi
else
    error ".gitmodules file not found"
fi

# 2. Check init-build.sh
echo ""
echo "2. Checking scripts/init-build.sh..."
if [ -f "scripts/init-build.sh" ]; then
    if grep -q "meta-openjdk-temurin" scripts/init-build.sh; then
        success "meta-openjdk-temurin layer clone code present in init-build.sh"
    else
        error "meta-openjdk-temurin layer clone code missing from init-build.sh"
    fi

    if grep -q "META_TEMURIN_COMMIT" scripts/init-build.sh; then
        success "META_TEMURIN_COMMIT variable defined"
    else
        error "META_TEMURIN_COMMIT variable missing"
    fi

    if bash -n scripts/init-build.sh 2>/dev/null; then
        success "init-build.sh syntax is valid"
    else
        error "init-build.sh has syntax errors"
    fi
else
    error "scripts/init-build.sh not found"
fi

# 3. Check setup-environment.sh
echo ""
echo "3. Checking setup-environment.sh..."
if [ -f "setup-environment.sh" ]; then
    if grep -q "meta-openjdk-temurin" setup-environment.sh; then
        success "meta-openjdk-temurin layer referenced in setup-environment.sh"
    else
        error "meta-openjdk-temurin layer not in bblayers.conf template"
    fi

    if bash -n setup-environment.sh 2>/dev/null; then
        success "setup-environment.sh syntax is valid"
    else
        error "setup-environment.sh has syntax errors"
    fi
else
    error "setup-environment.sh not found"
fi

# 4. Check packagegroup-star-ros2.bb
echo ""
echo "4. Checking packagegroup-star-ros2.bb..."
RECIPE="meta-star/recipes-core/packagegroups/packagegroup-star-ros2.bb"
if [ -f "$RECIPE" ]; then
    if grep -q "openjdk-17-jre" "$RECIPE"; then
        success "OpenJDK 17 JRE package included (from meta-openjdk-temurin)"
    else
        error "openjdk-17-jre package not found in recipe"
    fi

    if grep -q "meta-openjdk-temurin" "$RECIPE"; then
        success "Recipe references meta-openjdk-temurin layer"
    else
        warning "Recipe doesn't mention meta-openjdk-temurin (optional)"
    fi

    if grep -q "Kotlin" "$RECIPE" || grep -q "Java" "$RECIPE"; then
        success "Recipe description mentions Java/Kotlin support"
    else
        warning "Recipe description doesn't mention Java/Kotlin"
    fi

    # Basic BitBake syntax check
    if grep -q "^SUMMARY.*=" "$RECIPE" && \
       grep -q "^DESCRIPTION.*=" "$RECIPE" && \
       grep -q "^LICENSE.*=" "$RECIPE" && \
       grep -q "inherit packagegroup" "$RECIPE" && \
       grep -q "^RDEPENDS.*=" "$RECIPE"; then
        success "Recipe has required BitBake variables"
    else
        error "Recipe is missing required BitBake variables"
    fi
else
    error "packagegroup-star-ros2.bb not found"
fi

# 5. Check documentation
echo ""
echo "5. Checking documentation..."
if [ -f "BUILD_REQUIREMENTS.md" ]; then
    success "BUILD_REQUIREMENTS.md exists"

    if grep -q "OpenJDK 21" BUILD_REQUIREMENTS.md && \
       grep -q "Kotlin" BUILD_REQUIREMENTS.md; then
        success "BUILD_REQUIREMENTS.md mentions Java/Kotlin"
    else
        warning "BUILD_REQUIREMENTS.md may be missing Java/Kotlin info"
    fi
else
    error "BUILD_REQUIREMENTS.md not found"
fi

if [ -f "JAVA_KOTLIN_SETUP.md" ]; then
    success "JAVA_KOTLIN_SETUP.md exists"
else
    warning "JAVA_KOTLIN_SETUP.md not found (optional)"
fi

if [ -f "README.md" ]; then
    success "README.md exists"

    if grep -q "OpenJDK" README.md || grep -q "Java" README.md; then
        success "README.md mentions Java support"
    else
        warning "README.md doesn't mention Java support"
    fi
else
    error "README.md not found"
fi

# 6. Check layer structure
echo ""
echo "6. Checking meta-star layer structure..."
if [ -f "meta-star/conf/layer.conf" ]; then
    success "meta-star layer.conf exists"
else
    error "meta-star/conf/layer.conf not found"
fi

if [ -f "meta-star/recipes-core/images/star-minimal-image.bb" ]; then
    success "star-minimal-image.bb recipe exists"
else
    error "star-minimal-image.bb not found"
fi

# 7. Check scripts
echo ""
echo "7. Checking build scripts..."
if [ -f "scripts/build-image.sh" ]; then
    if bash -n scripts/build-image.sh 2>/dev/null; then
        success "build-image.sh syntax is valid"
    else
        error "build-image.sh has syntax errors"
    fi
else
    warning "scripts/build-image.sh not found"
fi

if [ -f "scripts/flash-sd.sh" ]; then
    if bash -n scripts/flash-sd.sh 2>/dev/null; then
        success "flash-sd.sh syntax is valid"
    else
        error "flash-sd.sh has syntax errors"
    fi
else
    warning "scripts/flash-sd.sh not found"
fi

# Summary
echo ""
echo "========================================="
echo "Validation Summary"
echo "========================================="

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo ""
    echo "Configuration is ready for building."
    echo ""
    echo "Next steps:"
    echo "  1. Initialize layers: ./scripts/init-build.sh"
    echo "  2. Setup environment: source setup-environment.sh"
    echo "  3. Build image: ./scripts/build-image.sh"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠ Validation completed with $WARNINGS warning(s)${NC}"
    echo ""
    echo "Configuration should work, but please review warnings."
    exit 0
else
    echo -e "${RED}✗ Validation failed with $ERRORS error(s) and $WARNINGS warning(s)${NC}"
    echo ""
    echo "Please fix errors before building."
    exit 1
fi
