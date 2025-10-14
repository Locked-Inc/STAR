# STAR-Z2 Build Instructions

Comprehensive guide to building custom PYNQ images for the STAR robot platform.

## Prerequisites

### 1. System Requirements
- Ubuntu 18.04 or 20.04 LTS (64-bit)
- 32GB+ RAM
- 200GB+ free disk space (SSD recommended)
- 8+ CPU cores (16+ recommended)
- High-speed internet connection

### 2. Installed Tools
- Xilinx Vivado/Vitis 2022.1 (see `XILINX_TOOLS_SETUP.md`)
- PetaLinux Tools 2022.1 (see `XILINX_TOOLS_SETUP.md`)
- Build dependencies (installed automatically by setup script)

## Quick Start (Prebuilt Image Method)

This is the fastest method, taking ~30-60 minutes:

```bash
# 1. Navigate to pynq-image directory
cd /home/bsikar/Documents/git/STAR/pynq-image

# 2. Setup environment
./scripts/setup-env.sh

# 3. Source Xilinx tools
source /opt/Xilinx/Vivado/2022.1/settings64.sh
source /opt/petalinux/2022.1/settings.sh

# 4. Download prebuilt base image (optional but faster)
# Get prebuilt ARM image from PYNQ releases or build server
wget <prebuilt-image-url> -O prebuilt-base.img

# 5. Build custom image
export PREBUILT_IMAGE=prebuilt-base.img
./scripts/build.sh prebuilt
```

## Full Build from Scratch

For complete control or when prebuilt images aren't available (2-4 hours):

```bash
# 1. Setup environment
cd /home/bsikar/Documents/git/STAR/pynq-image
./scripts/setup-env.sh

# 2. Source Xilinx tools
source /opt/Xilinx/Vivado/2022.1/settings64.sh
source /opt/petalinux/2022.1/settings.sh

# 3. Build from scratch
./scripts/build.sh full
```

## Build Process Details

### What Happens During Build

1. **Environment Setup** (1-2 min)
   - Validates system requirements
   - Sets up PYNQ build environment
   - Copies board configuration

2. **Base System Creation** (30-120 min, prebuilt vs full)
   - Creates Ubuntu root filesystem
   - Installs base packages
   - Configures system services

3. **PYNQ Installation** (10-20 min)
   - Installs PYNQ framework
   - Sets up Jupyter notebooks
   - Configures overlays

4. **Custom Packages** (10-30 min)
   - Installs robotics packages (LiDAR, vision, etc.)
   - Configures network services
   - Sets up robot control software

5. **Image Finalization** (5-10 min)
   - Creates bootable SD card image
   - Compresses final image
   - Generates checksums

### Build Output

After successful build:
```
pynq-image/PYNQ/sdbuild/output/
├── STAR-Z2-3.0.1.img        # SD card image (~8-10GB)
├── STAR-Z2-3.0.1.img.md5    # Checksum
└── build.log                # Build log
```

## Customization Options

### Add Custom Python Packages

Edit `board-config/STAR-Z2/STAR-Z2.spec`:
```makefile
STAGE4_PYTHON_PACKAGES_STAR-Z2 := numpy \
                                   scipy \
                                   opencv-python \
                                   your-package-here
```

### Add Custom System Packages

Edit `board-config/STAR-Z2/STAR-Z2.spec`:
```makefile
STAGE4_PACKAGES_STAR-Z2 := pynq \
                            ethernet \
                            your-package-here
```

### Add Custom Scripts

Place scripts in package directories:
```
board-config/STAR-Z2/packages/
├── lidar-slam/setup.sh       # LiDAR packages
├── vision/setup.sh           # Vision packages
├── network/setup.sh          # Network packages
└── robot-control/setup.sh    # Control packages
```

### Use Custom Hardware Design

1. Create your Vivado project
2. Export hardware (XSA file): File → Export → Export Hardware
3. Copy XSA file to: `board-config/STAR-Z2/petalinux_bsp/hardware_project/system.xsa`
4. Update `STAR-Z2.spec`:
```makefile
BSP_STAR-Z2 := petalinux_bsp/hardware_project/system.xsa
```

## Docker-Based Build (Reproducible)

For consistent builds across different machines:

```bash
cd /home/bsikar/Documents/git/STAR/pynq-image

# Build in Docker container
./scripts/docker/docker-build.sh prebuilt

# Or full build in Docker
./scripts/docker/docker-build.sh full
```

## Flashing to SD Card

### Linux

```bash
# Automatic method (safest)
sudo ./scripts/flash.sh

# Manual method
sudo dd if=PYNQ/sdbuild/output/STAR-Z2-3.0.1.img of=/dev/sdX bs=4M status=progress && sync
```

Replace `/dev/sdX` with your SD card device (check with `lsblk`)

### macOS

```bash
# Find disk identifier
diskutil list

# Unmount disk
diskutil unmountDisk /dev/diskX

# Flash image
sudo dd if=PYNQ/sdbuild/output/STAR-Z2-3.0.1.img of=/dev/rdiskX bs=4m && sync

# Eject
diskutil eject /dev/diskX
```

### Windows

Use [balenaEtcher](https://www.balena.io/etcher/) or [Rufus](https://rufus.ie/)

## First Boot

### Hardware Setup

1. Insert SD card into PYNQ-Z2
2. Set boot mode to SD:
   - Jumper JP4: Set to SD
3. Connect:
   - Ethernet cable (for network access)
   - USB cable (for UART console, optional)
   - Power supply (12V, 3A recommended)
   - HDMI (optional, shows IP address)

### Network Connection

The board will obtain an IP via DHCP. Find the IP address:

**Option 1**: HDMI display shows IP on boot

**Option 2**: Check DHMI server logs:
```bash
grep "PYNQ" /var/log/syslog
```

**Option 3**: Use hostname:
```bash
ping pynq
```

### SSH Access

```bash
ssh xilinx@<board-ip>
# Or
ssh xilinx@pynq

# Default credentials:
# Username: xilinx
# Password: xilinx
```

### Jupyter Notebook Access

Open browser:
```
http://<board-ip>:9090
# Or
http://pynq:9090

# Default password: xilinx
```

## Verification

After first boot, verify the installation:

```bash
# SSH into board
ssh xilinx@pynq

# Check PYNQ version
python3 -c "import pynq; print(pynq.__version__)"
# Should show: 3.0.1

# Check custom packages
pip3 list | grep opencv
pip3 list | grep numpy

# Check board info
cat /proc/device-tree/model
# Should show: Digilent Pynq-Z2

# Check available overlays
ls /home/xilinx/pynq/overlays/

# Test base overlay
python3 -c "from pynq import Overlay; ol = Overlay('base.bit'); print('Overlay loaded')"
```

## Troubleshooting

### Build Fails: "Disk space"

```bash
# Clean previous builds
cd pynq-image/PYNQ
make clean

# Check space
df -h .
```

### Build Fails: "Package not found"

```bash
# Update package lists
sudo apt-get update

# Reinstall dependencies
./scripts/setup-env.sh
```

### Build Fails: "Xilinx tools not found"

```bash
# Verify tool installation
source /opt/Xilinx/Vivado/2022.1/settings64.sh
vivado -version

source /opt/petalinux/2022.1/settings.sh
echo $PETALINUX
```

### Board Won't Boot

1. Check boot mode jumper (JP4 set to SD)
2. Re-flash SD card
3. Try different SD card (8GB+ Class 10 required)
4. Check power supply (12V, 3A minimum)
5. Connect UART console to see boot messages:
   ```bash
   sudo screen /dev/ttyUSB1 115200
   ```

### Can't Connect to Board

1. Check network cable connection
2. Verify DHCP server is running
3. Try direct connection with static IP:
   - Set board IP: `sudo ifconfig eth0 192.168.1.10`
   - Set PC IP: `192.168.1.11`
   - Connect: `ssh xilinx@192.168.1.10`

### Custom Packages Not Installed

Check package installation logs:
```bash
# On board
ssh xilinx@pynq
cat /var/log/pynq-install.log
```

## Build Performance Tips

### Optimize Thread Count

```bash
# Set based on CPU cores (leave 1-2 cores free)
export BB_NUMBER_THREADS=$(($(nproc) - 2))
export PARALLEL_MAKE="-j$(($(nproc) - 2))"
```

### Use Local Package Cache

```bash
# Set persistent cache directories
export DL_DIR=/opt/pynq-cache/downloads
export SSTATE_DIR=/opt/pynq-cache/sstate-cache

mkdir -p $DL_DIR $SSTATE_DIR
```

### Monitor Build Progress

```bash
# In separate terminal
watch -n 5 'df -h . && free -h'

# Or use htop
sudo apt-get install htop
htop
```

## Advanced Topics

### Creating Custom Overlays

See `docs/CUSTOM_OVERLAYS.md` (to be created)

### Modifying Kernel Configuration

See `docs/KERNEL_CUSTOMIZATION.md` (to be created)

### Setting Up ROS Integration

See `docs/ROS_INTEGRATION.md` (to be created)

## References

- [PYNQ Documentation](https://pynq.readthedocs.io/)
- [PYNQ sdbuild Guide](https://pynq.readthedocs.io/en/latest/pynq_sd_card.html)
- [PetaLinux Tools Reference](https://docs.xilinx.com/r/en-US/ug1144-petalinux-tools-reference-guide)
- [PYNQ Community Forum](https://discuss.pynq.io/)

## Getting Help

If you encounter issues:

1. Check `docs/TROUBLESHOOTING.md`
2. Review build logs in `PYNQ/sdbuild/build/`
3. Search [PYNQ Forum](https://discuss.pynq.io/)
4. Open an issue in this repository
