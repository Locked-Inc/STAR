#!/bin/bash

# This script configures and starts the Yocto build for the custom PYNQ image.

# Exit on error
set -e

# --- Configuration ---
# The ROS 2 distribution to install
export ROS_DISTRO="jazzy"
# The ROS 2 packages to install
export ROS_PACKAGES="ros-base ros-dev-tools"

# --- Script Start ---

# 1. Check if the build environment is sourced
if [ -z "$BUILDDIR" ]; then
  echo "Error: Yocto build environment not sourced."
  echo "Please run 'source sources/poky/oe-init-build-env' first."
  exit 1
fi

# 2. Add required layers to bblayers.conf
# This file is created when you source the environment script.
BBLAYERS_CONF=$BUILDDIR/conf/bblayers.conf

echo "Configuring $BBLAYERS_CONF..."

cat <<EOF > $BBLAYERS_CONF
# POKY_BBLAYERS_CONF_VERSION is increased each time build/conf/bblayers.conf
# changes incompatibly
POKY_BBLAYERS_CONF_VERSION = "2"

BBPATH = "$TOPDIR"
BBFILES ?= ""

BBLAYERS ?= " \
  $TOPDIR/../sources/poky/meta \
  $TOPDIR/../sources/poky/meta-poky \
  $TOPDIR/../sources/poky/meta-yocto-bsp \
  $TOPDIR/../sources/meta-xilinx/meta-xilinx-bsp \
  $TOPDIR/../sources/meta-xilinx/meta-xilinx-fpga \
  $TOPDIR/../sources/meta-xilinx/meta-xilinx-vendor \
  $TOPDIR/../sources/meta-pynq \
  $TOPDIR/../sources/meta-openembedded/meta-oe \
  $TOPDIR/../sources/meta-openembedded/meta-python \
  $TOPDIR/../sources/meta-openembedded/meta-networking \
  $TOPDIR/../sources/meta-openembedded/meta-multimedia \
  $TOPDIR/../sources/meta-ros/meta-ros-common \
  $TOPDIR/../sources/meta-ros/meta-ros2 \
  $TOPDIR/../sources/meta-ros/meta-ros2-jazzy \
  "
EOF

# 3. Configure local.conf
LOCAL_CONF=$BUILDDIR/conf/local.conf

echo "Configuring $LOCAL_CONF..."

# Set the machine to pynq-z2
echo 'MACHINE ?= "pynq-z2"' >> $LOCAL_CONF

# Add ROS 2 packages
echo "IMAGE_INSTALL:append = \" ${ROS_PACKAGES}\"" >> $LOCAL_CONF

# Set the ROS 2 distribution
echo "ROS_DISTRO = \"${ROS_DISTRO}\"" >> $LOCAL_CONF

# Yocto project settings
echo 'DL_DIR ?= "$TOPDIR/downloads"' >> $LOCAL_CONF
echo 'SSTATE_DIR ?= "$TOPDIR/sstate-cache"' >> $LOCAL_CONF
echo 'TMPDIR = "$TOPDIR/tmp"' >> $LOCAL_CONF
echo 'PACKAGE_CLASSES ?= "package_rpm"' >> $LOCAL_CONF
echo 'SDKMACHINE ?= "x86_64"' >> $LOCAL_CONF
echo 'PATCHRESOLVE = "noop"' >> $LOCAL_CONF

# 4. Start the build
# The target `pynq-image-minimal` is defined in meta-pynq.
# This will build the entire image from source.

echo "Starting the build..."
bitbake pynq-image-minimal

echo "Build complete. The image is in tmp/deploy/images/pynq-z2/"
