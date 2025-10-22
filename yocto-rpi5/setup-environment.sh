#!/bin/bash
# Setup script for STAR Yocto build environment
# Source this script: source setup-environment.sh

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check if we're being sourced (not executed)
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "ERROR: This script must be sourced, not executed!"
    echo "Usage: source setup-environment.sh"
    exit 1
fi

# Yocto build directory
BUILD_DIR="${SCRIPT_DIR}/build"

# Initialize poky build environment
if [ ! -d "${SCRIPT_DIR}/poky" ]; then
    echo "ERROR: Poky layer not found!"
    echo "Please run './scripts/init-build.sh' first to clone required layers."
    return 1
fi

# Source the Yocto environment setup
echo "Setting up Yocto build environment..."
cd "${SCRIPT_DIR}"
source poky/oe-init-build-env "${BUILD_DIR}" > /dev/null

# Yocto's oe-init-build-env changes to build dir, go back to yocto root
cd "${SCRIPT_DIR}"

# Create conf directory if it doesn't exist
mkdir -p "${BUILD_DIR}/conf"

# Setup bblayers.conf if it doesn't exist or doesn't have our custom layers
if [ ! -f "${BUILD_DIR}/conf/bblayers.conf" ] || ! grep -q "meta-raspberrypi" "${BUILD_DIR}/conf/bblayers.conf" 2>/dev/null; then
    echo "Configuring build layers..."
    cat > "${BUILD_DIR}/conf/bblayers.conf" << EOF
# POKY_BBLAYERS_CONF_VERSION is increased each time build/conf/bblayers.conf
# changes incompatibly
POKY_BBLAYERS_CONF_VERSION = "2"

BBPATH = "\${TOPDIR}"
BBFILES ?= ""

BBLAYERS ?= " \\
  ${SCRIPT_DIR}/poky/meta \\
  ${SCRIPT_DIR}/poky/meta-poky \\
  ${SCRIPT_DIR}/poky/meta-yocto-bsp \\
  ${SCRIPT_DIR}/meta-openembedded/meta-oe \\
  ${SCRIPT_DIR}/meta-openembedded/meta-python \\
  ${SCRIPT_DIR}/meta-openembedded/meta-networking \\
  ${SCRIPT_DIR}/meta-raspberrypi \\
  ${SCRIPT_DIR}/meta-java \\
  ${SCRIPT_DIR}/meta-star \\
  "
EOF
fi

# Configure local.conf with minimal settings
if [ ! -f "${BUILD_DIR}/conf/local.conf.bak" ]; then
    # Backup original if it exists
    if [ -f "${BUILD_DIR}/conf/local.conf" ]; then
        cp "${BUILD_DIR}/conf/local.conf" "${BUILD_DIR}/conf/local.conf.bak"
    fi

    echo "Configuring build settings..."
    cat > "${BUILD_DIR}/conf/local.conf" << 'EOF'
#
# STAR Minimal Build Configuration
#

# Machine Selection
MACHINE = "raspberrypi5"

# Distribution
DISTRO = "poky"

# Build parallelization
BB_NUMBER_THREADS ?= "${@oe.utils.cpu_count()}"
PARALLEL_MAKE ?= "-j ${@oe.utils.cpu_count()}"

# Disk Space Monitoring
BB_DISKMON_DIRS ??= "\
    STOPTASKS,${TMPDIR},1G,100K \
    STOPTASKS,${DL_DIR},1G,100K \
    STOPTASKS,${SSTATE_DIR},1G,100K \
    STOPTASKS,/tmp,100M,100K \
    HALT,${TMPDIR},100M,1K \
    HALT,${DL_DIR},100M,1K \
    HALT,${SSTATE_DIR},100M,1K \
    HALT,/tmp,10M,1K"

# Shared state cache location
SSTATE_DIR ?= "${TOPDIR}/../sstate-cache"

# Download directory
DL_DIR ?= "${TOPDIR}/../downloads"

# Package Management
PACKAGE_CLASSES ?= "package_ipk"
EXTRA_IMAGE_FEATURES ?= "debug-tweaks"

# Locale
IMAGE_LINGUAS = "en-us"

# Timezone
DEFAULT_TIMEZONE = "UTC"

# Init Manager - using systemd (well-tested, reliable)
INIT_MANAGER = "systemd"

# Disable unnecessary features for minimal build
DISTRO_FEATURES:remove = "x11 wayland vulkan opengl 3g nfc bluetooth irda pcmcia nfs zeroconf pci pulseaudio"

# Keep essential features
DISTRO_FEATURES:append = " wifi ipv4 ipv6"

# Minimal kernel features
KERNEL_FEATURES:remove = "features/netfilter/netfilter.scc features/taskstats/taskstats.scc"

# Remove unnecessary packages from core-boot
BAD_RECOMMENDATIONS:append = " busybox-syslog"

# Accept licenses
LICENSE_FLAGS_ACCEPTED = "commercial synaptics-killswitch"

# Error reporting
INHERIT += "rm_work"

# Save disk space by removing work files after build
# Comment out if you need to debug build issues
# RM_WORK_EXCLUDE += "star-minimal-image"

# Additional custom settings
CONF_VERSION = "2"
EOF
fi

# Set helpful environment variables
export YOCTO_ROOT="${SCRIPT_DIR}"
export DOWNLOADS_DIR="${SCRIPT_DIR}/downloads"
export SSTATE_DIR="${SCRIPT_DIR}/sstate-cache"

# Create directories if they don't exist
mkdir -p "${DOWNLOADS_DIR}"
mkdir -p "${SSTATE_DIR}"

echo ""
echo "========================================="
echo "Yocto Build Environment Ready!"
echo "========================================="
echo "Build directory: ${BUILD_DIR}"
echo "Machine: raspberrypi5"
echo "Distribution: Minimal (Poky-based)"
echo ""
echo "To build the minimal image:"
echo "  ./scripts/build-image.sh"
echo ""
echo "Or manually:"
echo "  bitbake star-minimal-image"
echo ""
echo "Configuration files:"
echo "  ${BUILD_DIR}/conf/local.conf"
echo "  ${BUILD_DIR}/conf/bblayers.conf"
echo "========================================="
echo ""
