# STAR-Z2 Quick Start Guide

Get your STAR robot running in 4 steps!

## Step 1: Install Xilinx Tools

You need Vivado/Vitis and PetaLinux 2022.1. This is a one-time setup.

```bash
# See detailed instructions
cat docs/XILINX_TOOLS_SETUP.md

# Quick summary:
# 1. Download from xilinx.com (requires account)
# 2. Install Vivado/Vitis 2022.1 to /opt/Xilinx
# 3. Install PetaLinux 2022.1 to /opt/petalinux
# 4. Add to ~/.bashrc:
#    source /opt/Xilinx/Vivado/2022.1/settings64.sh
#    source /opt/petalinux/2022.1/settings.sh
```

**Time required**: 2-3 hours (one-time setup)

## Step 2: Build Image

```bash
# Navigate to directory
cd /home/bsikar/Documents/git/STAR/pynq-image

# Setup environment (installs dependencies)
./scripts/setup-env.sh

# Source Xilinx tools
source ~/.bashrc

# Build image (prebuilt method - 30-60 minutes)
./scripts/build.sh full

# OR with prebuilt base (faster):
# export PREBUILT_IMAGE=/path/to/prebuilt.img
# ./scripts/build.sh prebuilt
```

**Time required**: 30 minutes (prebuilt) or 2-4 hours (full build)

## Step 3: Flash SD Card

```bash
# Automatic (recommended)
sudo ./scripts/flash.sh

# Follow prompts to select SD card device
```

**Time required**: 10-15 minutes

## Step 4: Boot and Connect

1. **Hardware**:
   - Insert SD card into PYNQ-Z2
   - Set boot jumper (JP4) to SD
   - Connect Ethernet cable
   - Power on (12V, 3A)

2. **Connect**:
   ```bash
   # Wait 1-2 minutes for boot
   ssh star@192.168.2.99
   # Password: star
   # Or with alias: ssh star
   ```

**Time required**: 2-5 minutes

## Verify Installation

```bash
# SSH into board
ssh star@192.168.2.99

# Check PYNQ
python3 -c "import pynq; print(f'PYNQ {pynq.__version__}')"

# Check custom packages
pip3 list | grep -E "(opencv|numpy|flask)"

# Test base overlay
python3 -c "from pynq import Overlay; ol = Overlay('base.bit'); print('✓ Overlay loaded')"
```

## Next Steps

- **Develop Custom Overlays**: See `docs/BUILD_INSTRUCTIONS.md`
- **Integrate LiDAR**: Configure in `board-config/STAR-Z2/packages/lidar-slam/`
- **Setup Cameras**: Configure in `board-config/STAR-Z2/packages/vision/`
- **Deploy Robot Gateway**: See `../RobotGateway/README.md`

## Troubleshooting

### Build fails
```bash
# Check disk space (need 200GB+)
df -h .

# Check RAM (need 16GB+)
free -h

# Check Xilinx tools
vivado -version
echo $PETALINUX
```

### Board won't boot
- Verify boot jumper (JP4) set to SD
- Try different SD card (8GB+ Class 10)
- Check power supply (12V, 3A minimum)
- Connect UART console: `screen /dev/ttyUSB1 115200`

### Can't connect
- Check Ethernet cable
- Try hostname: `ping pynq`
- Check HDMI output for IP address
- Try direct connection with static IP

## Getting Help

- Documentation: `docs/BUILD_INSTRUCTIONS.md`
- Troubleshooting: `docs/TROUBLESHOOTING.md` (if exists)
- PYNQ Forum: https://discuss.pynq.io/
- Project Issues: GitHub issues

## System Overview

```
STAR Robot Stack:
┌─────────────────────────────────────┐
│  Handheld Controller (Retroid)      │
└───────────┬─────────────────────────┘
            │ WiFi
┌───────────▼─────────────────────────┐
│  Robot Gateway (Java/Spring Boot)   │
└───────────┬─────────────────────────┘
            │ Commands
┌───────────▼─────────────────────────┐
│  PYNQ-Z2 Custom Linux (This Image)  │
│  ┌─────────────────────────────┐   │
│  │ LiDAR SLAM Processing       │   │
│  │ Stereo Vision (OpenCV)      │   │
│  │ Motor Control (FPGA)        │   │
│  │ Network Services            │   │
│  └─────────────────────────────┘   │
└───────────┬─────────────────────────┘
            │ Hardware I/O
    ┌───────┴────────┐
    │                │
┌───▼───┐      ┌────▼────┐
│ LiDAR │      │ Cameras │
└───────┘      └─────────┘
```

## Quick Reference

| Component | Path | Purpose |
|-----------|------|---------|
| Board Config | `board-config/STAR-Z2/` | Custom board specification |
| Build Scripts | `scripts/` | Build automation |
| PYNQ Source | `PYNQ/` | PYNQ framework (submodule) |
| Output Images | `PYNQ/sdbuild/output/` | Built SD card images |
| Documentation | `docs/` | Detailed guides |

## Build Options Summary

```bash
# Prebuilt (fast, recommended)
export PREBUILT_IMAGE=/path/to/base.img
./scripts/build.sh prebuilt

# Full build (slow, complete control)
./scripts/build.sh full

# Docker build (reproducible)
./scripts/docker/docker-build.sh prebuilt
```

**Enjoy building your STAR robot!**
