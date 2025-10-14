# PYNQ Image Customization Guide

## Can You Add/Remove Features? YES!

The PYNQ build system allows full customization of packages and features.

## Understanding the Package System

The PYNQ build uses **staged packages**:

### Package Stages:
- **STAGE1**: Base Ubuntu root filesystem (debootstrap)
- **STAGE2**: Architecture-specific base packages (ARM/AARCH64)
- **STAGE3**: Architecture-specific PYNQ core packages
- **STAGE4**: Board-specific packages (defined in `.spec` files)

### Package Structure:
Packages are located in `pynq-image/PYNQ/sdbuild/packages/`

Each package can have:
- `Makefile` - build dependencies (runs on host)
- `pre.sh` - runs before chroot (copies files into chroot)
- `qemu.sh` - runs inside chroot under QEMU (installs/configures)
- `post.sh` - runs after chroot (cleanup)

## Current STAR-Z2 Image Packages

### Base System (STAGE1):
- Ubuntu 22.04 Jammy (armhf)
- Build tools, compilers, Python
- X11 libraries (for development)
- Scientific libraries (numpy, scipy, opencv)
- Development tools

### ARM Architecture Packages (STAGE2):
Located in: `PYNQ/sdbuild/ubuntu/jammy/arm/config`
```makefile
STAGE2_PACKAGES_arm := gcc-mb ssl udev python_packages_jammy libsds
STAGE2_PACKAGES_arm += jupyter sigrok
STAGE2_PACKAGES_arm += pybind11
STAGE2_PACKAGES_arm += bootpy
STAGE2_PACKAGES_arm += clear_pl_statefile
```

**What they do:**
- `gcc-mb`: MicroBlaze GCC cross-compiler (for soft processors in FPGA)
- `ssl`: SSL certificates
- `udev`: Device manager
- `python_packages_jammy`: Python ecosystem
- `jupyter`: Jupyter Notebook server ← You asked about this!
- `sigrok`: Logic analyzer/oscilloscope support
- `pybind11`: C++/Python bindings
- `bootpy`: Boot Python scripts
- `clear_pl_statefile`: FPGA programming logic cleanup

### PYNQ Core Packages (STAGE3):
```makefile
STAGE3_PACKAGES_arm := xrtlib pynq resizefs
```

**What they do:**
- `xrtlib`: Xilinx Runtime Library
- `pynq`: Main PYNQ Python package
- `resizefs`: Filesystem auto-resizing on first boot

### Board-Specific (STAGE4):
From your `STAR-Z2.spec`:
```makefile
STAGE4_PACKAGES_STAR-Z2 := pynq \
                            ethernet \
                            boot_leds \
                            opencv \
                            network_manager \
                            robotics_tools
```

**Note:** Some of these packages (`boot_leds`, `network_manager`, `robotics_tools`) don't exist yet in the packages directory.

## How to Remove Jupyter Notebook

### Step 1: Edit ARM Configuration

**File:** `PYNQ/sdbuild/ubuntu/jammy/arm/config`

**Change:**
```makefile
# Before:
STAGE2_PACKAGES_arm := gcc-mb ssl udev python_packages_jammy libsds
STAGE2_PACKAGES_arm += jupyter sigrok
STAGE2_PACKAGES_arm += pybind11

# After:
STAGE2_PACKAGES_arm := gcc-mb ssl udev python_packages_jammy libsds
STAGE2_PACKAGES_arm += sigrok
STAGE2_PACKAGES_arm += pybind11
```

Simply remove `jupyter` from line 2.

### Step 2: Rebuild Image
```bash
cd ~/STAR/pynq-image
make BOARDS=STAR-Z2
```

That's it! Jupyter will no longer be installed.

## How to Add ROS Support

### Option A: Create Custom ROS Package (Recommended)

#### Step 1: Create Package Structure
```bash
cd ~/STAR/pynq-image/PYNQ/sdbuild/packages
mkdir ros
cd ros
```

#### Step 2: Create `qemu.sh` (Installation Script)
```bash
nano qemu.sh
```

```bash
#!/bin/bash
set -e
set -x

# Source environment
for f in /etc/profile.d/*.sh; do source $f; done

# Add ROS 2 repository
apt-get update
apt-get install -y software-properties-common curl
curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | tee /etc/apt/sources.list.d/ros2.list > /dev/null

# Install ROS 2 Humble (matches Ubuntu 22.04 Jammy)
apt-get update
apt-get install -y ros-humble-ros-base

# Install common ROS packages
apt-get install -y \
    ros-humble-sensor-msgs \
    ros-humble-geometry-msgs \
    ros-humble-urg-node \
    ros-humble-sick-scan \
    python3-colcon-common-extensions

# Set up environment
echo "source /opt/ros/humble/setup.bash" >> /etc/profile.d/ros_setup.sh

# Create workspace directory
mkdir -p /home/xilinx/ros2_ws/src
chown -R 1000:1000 /home/xilinx/ros2_ws
```

#### Step 3: Make Executable
```bash
chmod +x qemu.sh
```

#### Step 4: Add to Board Spec
Edit `board-config/STAR-Z2/STAR-Z2.spec`:

```makefile
STAGE4_PACKAGES_STAR-Z2 := pynq \
                            ethernet \
                            boot_leds \
                            opencv \
                            network_manager \
                            robotics_tools \
                            ros
```

#### Step 5: Rebuild
```bash
cd ~/STAR/pynq-image
make BOARDS=STAR-Z2
```

### Option B: Install ROS After Boot (Faster for Testing)

```bash
# Boot your board, SSH in
ssh xilinx@board

# Add ROS repository
sudo apt update
sudo apt install -y software-properties-common curl
curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

# Install ROS 2
sudo apt update
sudo apt install -y ros-humble-ros-base

# Install lidar drivers
sudo apt install -y ros-humble-urg-node ros-humble-sick-scan

# Add to bashrc
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc

# Everything persists on SD card!
```

## What's Currently Installed in Your Image

### Development Tools:
- gcc, g++, make, cmake
- git, vim, nano
- gdb, strace, valgrind

### Python Ecosystem:
- Python 3.10
- NumPy, SciPy, Matplotlib
- OpenCV, scikit-image
- Jupyter Notebook (if not removed)

### System Tools:
- SSH server
- NetworkManager alternatives
- systemd
- Bluetooth (bluez)

### FPGA/Embedded:
- PYNQ Python library
- Xilinx tools integration
- i2c-tools, devmem2
- MicroBlaze cross-compiler

### GUI (libraries only, no X11 server):
- X11 development libraries
- GTK2 libraries
- Fluxbox (installed but not configured)

**Note:** There's NO X11 server or desktop environment configured for ARM by default. That's only on AARCH64 (Zynq UltraScale+).

## Creating Your Own Custom Package

Example: Let's create a `robot_tools` package

### Step 1: Create Package Directory
```bash
cd ~/STAR/pynq-image/PYNQ/sdbuild/packages
mkdir robot_tools
cd robot_tools
```

### Step 2: Create `pre.sh` (Copy Files)
```bash
nano pre.sh
```

```bash
#!/bin/bash
target=$1
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Copy your custom scripts
sudo cp $script_dir/robot_startup.sh $target/usr/local/bin/
sudo cp $script_dir/robot.service $target/lib/systemd/system/
sudo chmod +x $target/usr/local/bin/robot_startup.sh
```

### Step 3: Create `qemu.sh` (Install in Chroot)
```bash
nano qemu.sh
```

```bash
#!/bin/bash
set -e
set -x

# Install any additional packages
apt-get update
apt-get install -y can-utils python3-can

# Enable systemd service
systemctl enable robot.service

# Create directories
mkdir -p /home/xilinx/robot_data
chown 1000:1000 /home/xilinx/robot_data
```

### Step 4: Create Supporting Files
```bash
nano robot_startup.sh
```

```bash
#!/bin/bash
echo "STAR Robot starting up..."
# Your robot initialization code here
```

```bash
nano robot.service
```

```ini
[Unit]
Description=STAR Robot Startup
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/robot_startup.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

### Step 5: Make Scripts Executable
```bash
chmod +x pre.sh qemu.sh robot_startup.sh
```

### Step 6: Add to Your Board Spec
```makefile
STAGE4_PACKAGES_STAR-Z2 := pynq ethernet robot_tools
```

## Build Time Considerations

### First Build:
- **Hours** - Downloads Ubuntu packages, builds toolchains
- **With REBUILD flags**: Even longer

### Incremental Builds:
- If you only change board-specific packages: **Minutes**
- If you change STAGE2/STAGE3: **Hours** (rebuilds rootfs)

### Speed Up Development:
1. **Test on running board first** (packages persist on SD card)
2. **Once stable, add to image build**
3. **Use `REBUILD_PYNQ_ROOTFS=0`** for faster board-only rebuilds

## Summary: Customization Strategy

### For Testing/Development:
```bash
# SSH into board, install directly
sudo apt install some-package
# Test it
# If it works, add to image build
```

### For Production/Deployment:
```bash
# Add to appropriate package stage
# Rebuild image
make BOARDS=STAR-Z2
# Flash to SD card for clean deployment
```

### Package Stage Guidelines:
- **STAGE2**: Core architecture packages everyone needs
- **STAGE3**: PYNQ-specific packages
- **STAGE4**: Board-specific customizations

### Memory Considerations:
- **ROS adds ~500MB-1GB** depending on packages
- **Remove Jupyter saves ~200-300MB**
- **Remove X11 package (if added) saves ~100MB**
- Monitor with: `df -h`
