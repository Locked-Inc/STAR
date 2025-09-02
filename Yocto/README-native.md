# Yocto Build Environment Setup - Native Linux

This document describes how to set up the Yocto build environment directly on a native Linux system for building images for the TDA4VM Dev Board.

## Prerequisites

- Ubuntu 20.04 LTS or later (or compatible Linux distribution)
- At least 100GB free disk space
- At least 8GB RAM (16GB+ recommended)
- Internet connection for downloading sources

## 1. Native Linux Setup

Run the automated setup script directly on your Linux system:

```bash
chmod +x setup-yocto-j721e-native.sh
./setup-yocto-j721e-native.sh
```

This script will:
- Install all required build dependencies
- Create a Yocto build environment in `~/yocto-j721e-build`
- Configure the environment for J721E (TDA4VM) builds
- Optimize build settings based on your system's CPU cores

## 2. Create Non-Root User (Required)

Bitbake cannot be run as root. Create a dedicated user for Yocto builds:

```bash
# Create yocto user
sudo useradd -m -s /bin/bash yocto

# Move build directory to user home (if run from root initially)
sudo mv ~/yocto-j721e-build /home/yocto/
sudo chown -R yocto:yocto /home/yocto/yocto-j721e-build

# Switch to yocto user for all build commands
sudo su - yocto
```

## 3. Build Yocto Image for J721E TDA4VM

### Option 1: Using the Build Script (Recommended)

```bash
# Run the automated build script
./run-yocto-build.sh
```

This script will automatically:
- Switch to the yocto user if running as root
- Navigate to the correct build directory
- Source the environment
- Start the bitbake build

### Option 2: Manual Build

After setup is complete and as the yocto user, build the image:

```bash
# Switch to yocto user (if running as root)
sudo su - yocto

# Navigate to build directory
cd ~/yocto-j721e-build/oe-layersetup/build

# Source the build environment
source conf/setenv

# Build ADAS image for J721E (TDA4VM)
bitbake tisdk-adas-image
```

### Available Image Targets:
- `tisdk-adas-image`: Full ADAS filesystem with AI/ML capabilities
- `tisdk-edgeai-image`: EdgeAI optimized filesystem
- `tisdk-default-image`: Default filesystem
- `tisdk-base-image`: Minimal filesystem

### Build Output:
The built images will be located in:
```
~/yocto-j721e-build/oe-layersetup/build/arago-tmp-external-arm-glibc/deploy/images/j721e-evm/
```

## 4. Build Optimizations

The native setup automatically optimizes build settings:
- Uses all available CPU cores for parallel building
- Sets up shared download and state cache directories
- Configures optimal thread counts

## 5. Adding Custom Packages and Scripts

To add your custom packages and scripts to the image:

1. Create a custom meta-layer:
```bash
# From the build directory
mkdir -p ../meta-custom/recipes-custom/custom-scripts
```

2. Add your recipes and files to the custom layer
3. Add the layer to `bblayers.conf`
4. Rebuild the image

## 6. Troubleshooting

### Common Issues:
- **Root user error**: Bitbake cannot run as root - follow the user creation steps above
- **Permission denied errors**: Ensure all paths in config files point to `/home/yocto/` instead of `/root/`
- **Path errors in config**: Check `conf/setenv`, `conf/bblayers.conf`, and `conf/local.conf` for hardcoded `/root/` paths
- **Disk space**: Ensure at least 100GB free space
- **Memory**: Build may fail with less than 8GB RAM
- **Network**: Some packages require internet access during build

### Clean rebuild:
```bash
cd ~/yocto-j721e-build/oe-layersetup/build
source conf/setenv
bitbake -c cleanall tisdk-adas-image
bitbake tisdk-adas-image
```

## Notes:
- The J721E machine configuration supports the TDA4VM SoC and J721EXSKG01EVM board
- Build time can be 1-3 hours depending on system performance and image complexity
- First build downloads ~20GB of sources and takes longer
- Subsequent builds use cached data and are much faster