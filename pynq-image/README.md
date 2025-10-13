# STAR Robot - PYNQ-Z2 Custom Image Build System

This directory contains the custom Linux image build system for the STAR robot's PYNQ-Z2 board.

## Approach: PYNQ sdbuild with Prebuilt Images

We use the **prebuilt image approach** which provides:
- Fast build times (30 minutes vs 3+ hours)
- Stability and compatibility with PYNQ ecosystem
- Easy customization for robotics applications
- Lower hardware requirements for build machine

## Build System Requirements

### Host System
- **OS**: Ubuntu 18.04 or 20.04 LTS (NOT 22.04+)
- **CPU**: 4+ cores (8+ recommended)
- **RAM**: 16GB minimum (32GB recommended)
- **Storage**: 200GB free space (SSD recommended)
- **Network**: High-speed internet for package downloads

### Required Tools
- Xilinx Vivado/Vitis 2020.2-2022.1 (version must match PYNQ version)
- PetaLinux Tools (same version as Vivado)
- Git with LFS support
- Docker (optional but recommended for reproducible builds)

## Quick Start

### 1. Environment Setup

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y git build-essential libncurses5-dev \
    zlib1g-dev gawk flex bison libssl-dev bc python3-pip \
    chrpath socat cpio python3-pexpect xz-utils debianutils \
    iputils-ping python3-git python3-jinja2 libegl1-mesa \
    libsdl1.2-dev pylint3 xterm git-lfs

# Clone PYNQ repository
git clone --recursive https://github.com/Xilinx/PYNQ.git
cd PYNQ
git checkout v3.0.1  # Stable release
git submodule update --init --recursive

# Setup build environment
source ./sdbuild/scripts/setup_host.sh
```

### 2. Download Prebuilt Image

```bash
# Download prebuilt ARM image for faster builds
cd sdbuild/prebuilt
# Download from PYNQ releases or build server
# wget <prebuilt-image-url>
```

### 3. Configure Custom Board

```bash
# Copy board configuration
cp -r boards/Pynq-Z2 boards/STAR-Z2

# Edit board specification
vim boards/STAR-Z2/STAR-Z2.spec
```

### 4. Build Custom Image

```bash
# Build with prebuilt image (30 minutes)
make PREBUILT=<path-to-prebuilt.img> BOARDS=STAR-Z2

# OR full build from scratch (2-4 hours)
make BOARDS=STAR-Z2
```

### 5. Flash to SD Card

```bash
cd sdbuild/output
sudo dd if=STAR-Z2-3.0.1.img of=/dev/sdX bs=4M status=progress && sync
```

## Project Structure

```
pynq-image/
├── README.md                 # This file
├── docs/
│   ├── BUILD_SETUP.md       # Detailed setup instructions
│   ├── CUSTOMIZATION.md     # How to customize packages
│   └── TROUBLESHOOTING.md   # Common issues and solutions
├── board-config/
│   ├── STAR-Z2.spec         # Board specification
│   ├── packages/            # Custom package recipes
│   │   ├── lidar-slam/     # LiDAR SLAM packages
│   │   ├── vision/         # Computer vision packages
│   │   └── network/        # Network utilities
│   └── overlays/           # Custom FPGA overlays
├── scripts/
│   ├── setup-env.sh        # Environment setup script
│   ├── build.sh            # Main build script
│   ├── flash.sh            # SD card flashing script
│   └── docker/             # Docker build environment
└── .github/
    └── workflows/
        └── build-image.yml  # CI/CD workflow
```

## Custom Packages for STAR Robot

### LiDAR SLAM
- `slam-toolbox`: ROS SLAM package
- `hector-slam`: Alternative SLAM implementation
- Custom lightweight SLAM libraries

### Computer Vision
- `opencv-python`: Computer vision library
- `pynq-computervision`: FPGA-accelerated image processing
- Stereo camera drivers (ZED, RealSense)

### Network Communication
- `networkmanager`: Network configuration
- Custom WiFi setup utilities
- Real-time communication protocols

### Robot Control
- `robot-gateway`: Java Spring Boot gateway
- Motor control libraries
- Sensor integration packages

## Build Time Estimates

| Configuration | Time | RAM | Storage |
|--------------|------|-----|---------|
| Prebuilt (minimal) | 30 min | 16GB | 100GB |
| Prebuilt (full) | 1 hour | 16GB | 150GB |
| Full build (4 cores) | 4-6 hours | 16GB | 200GB |
| Full build (8+ cores) | 2-3 hours | 32GB | 200GB |

## Version Control Strategy

- **main**: Stable, tested configurations
- **develop**: Integration branch for new features
- **feature/**: Individual feature development
- **release/**: Release preparation

### Versioning
- Format: `vMAJOR.MINOR.PATCH`
- Example: `v1.0.0` (major), `v1.1.0` (minor), `v1.1.1` (patch)

## CI/CD Integration

GitHub Actions automatically builds images on:
- Push to main/develop branches
- Pull requests
- Tagged releases

Build artifacts are stored for 30 days and can be flashed directly to SD cards.

## Docker-based Builds (Recommended)

```bash
# Build in Docker for reproducibility
cd scripts/docker
docker build -t star-pynq-builder .
docker run -v $(pwd)/../..:/workspace star-pynq-builder
```

## Troubleshooting

### Build fails on Ubuntu 22.04
**Solution**: Use Ubuntu 18.04 or 20.04 LTS. Ubuntu 22.04+ has compatibility issues.

### Tool version mismatches
**Solution**: Ensure Vivado, Vitis, and PetaLinux versions match exactly.

### Disk space issues
**Solution**: Ensure 200GB+ free space. Clean intermediate files with `make clean`.

### Package download timeouts
**Solution**: Configure apt retries and use local mirrors if possible.

### Memory errors during build
**Solution**: Add swap space or increase RAM to 32GB.

For detailed troubleshooting, see `docs/TROUBLESHOOTING.md`.

## Migration Path to Yocto (Future)

AMD is transitioning from PetaLinux to native Yocto Project. When ready to migrate:
1. Export current configuration
2. Set up Yocto environment with meta-xilinx layers
3. Migrate custom packages to Yocto recipes
4. Test thoroughly before production deployment

## References

- [PYNQ Documentation](https://pynq.readthedocs.io/)
- [PYNQ GitHub Repository](https://github.com/Xilinx/PYNQ)
- [PYNQ Community Forum](https://discuss.pynq.io/)
- [AMD Yocto Documentation](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/2907766785/Yocto+Project+Machine+Configuration+Support)
- [PYNQ sdbuild Tutorial](https://discuss.pynq.io/t/pynq-2-6-custom-board-image-build-method-that-works/2894)

## Support

For issues and questions:
- Check `docs/TROUBLESHOOTING.md`
- Search [PYNQ Forum](https://discuss.pynq.io/)
- Open an issue in this repository
