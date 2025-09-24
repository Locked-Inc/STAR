# PYNQ-Z2 Robot System - SD Card Flash Instructions

## 📋 Prerequisites

After completing the Linux build, you should have:
- `pynq-robot-image-zynq-generic.wic` (or `core-image-minimal-qemuarm.wic`)
- 32GB+ microSD card (Class 10 or better)
- SD card reader
- Linux system (or WSL2)

## 🔍 Locate Your Build Artifacts

```bash
cd STAR/yocto-build
ls -la deploy/images/*/
```

Look for files ending in `.wic` - these are the flashable SD card images.

## 💾 Flash SD Card - Linux/WSL2

### 1. Insert SD Card and Find Device

```bash
# Before inserting SD card
lsblk

# Insert SD card, then run again to see new device
lsblk

# Usually appears as /dev/sdb, /dev/sdc, etc.
# WARNING: Double-check device name to avoid data loss!
```

### 2. Unmount SD Card (if auto-mounted)

```bash
# Unmount all partitions on the SD card
sudo umount /dev/sdX*  # Replace X with your device letter
```

### 3. Flash the Image

```bash
# Flash the WIC image to SD card
sudo dd if=deploy/images/zynq-generic/pynq-robot-image-zynq-generic.wic \
        of=/dev/sdX \
        bs=4M \
        status=progress \
        conv=fsync

# This will take 5-15 minutes depending on image size and SD card speed
```

### 4. Verify Flash

```bash
# Sync and eject
sync
sudo eject /dev/sdX

# Re-insert card and verify partitions
lsblk
```

You should see partitions like:
- Boot partition (FAT32, ~100MB)
- Root partition (ext4, remainder of card)

## 💾 Flash SD Card - macOS

### 1. Find SD Card Device

```bash
# List devices before inserting card
diskutil list

# Insert SD card and list again
diskutil list

# Look for new device like /dev/disk4
```

### 2. Unmount SD Card

```bash
# Unmount the disk (not eject)
diskutil unmountDisk /dev/diskN  # Replace N with your disk number
```

### 3. Flash Image

```bash
# Use raw disk device for faster writing
sudo dd if=deploy/images/zynq-generic/pynq-robot-image-zynq-generic.wic \
        of=/dev/rdiskN \
        bs=4m \
        conv=sync

# Note: 'rdiskN' (raw) is faster than 'diskN'
```

### 4. Eject Safely

```bash
diskutil eject /dev/diskN
```

## 💾 Flash SD Card - Windows

### Using Balena Etcher (Recommended)

1. Download [Balena Etcher](https://www.balena.io/etcher/)
2. Run Etcher as Administrator
3. Select your `.wic` image file
4. Select your SD card
5. Click "Flash!"

### Using Win32DiskImager

1. Download [Win32DiskImager](https://sourceforge.net/projects/win32diskimager/)
2. Run as Administrator
3. Select `.wic` file
4. Select SD card drive
5. Click "Write"

## 🔧 First Boot Setup

### 1. Hardware Setup

```bash
# PYNQ-Z2 connections:
# - microSD card in slot
# - Power via USB-C or barrel jack
# - Ethernet cable (for SSH access)
# - Optional: HDMI, USB keyboard/mouse
```

### 2. Power On

- Insert flashed SD card into PYNQ-Z2
- Connect power (LED should light up)  
- Wait 1-2 minutes for first boot (initial setup)

### 3. Network Access

**Option A: Ethernet (Recommended)**
```bash
# PYNQ-Z2 will get DHCP address
# Check router admin or use nmap to find IP
nmap -sP 192.168.1.0/24

# Look for device with hostname 'pynq' or 'robot'
```

**Option B: HDMI + Keyboard**
```bash
# Connect HDMI monitor and USB keyboard
# Login: root (no password initially)
# Check IP address: ip addr show
```

### 4. SSH Connection

```bash
# SSH to the robot (replace with actual IP)
ssh root@192.168.1.100

# First login should work without password
# Change password immediately:
passwd root
```

## 🤖 Robot System Verification

### Check Robot Services

```bash
# On the PYNQ-Z2, verify services are running:
systemctl status robot-gateway
systemctl status ros2-nodes

# Check hardware interfaces:
ls /dev/ttyS* /dev/i2c* /dev/spi*

# Verify Python installation:
python3 -c "import cv2, numpy; print('OpenCV and NumPy working')"
```

### Test Robot Functions

```bash
# Test ROS2 installation:
source /opt/ros/humble/setup.bash
ros2 node list

# Test camera (if connected):
v4l2-ctl --list-devices

# Test GPIO/hardware:
cat /proc/device-tree/model
```

## 🚨 Troubleshooting

### SD Card Won't Boot

**Check:**
- SD card properly inserted
- Power supply adequate (5V, 2A minimum)
- Boot switches in correct position
- SD card not corrupted

**Solutions:**
```bash
# Re-flash with verify flag:
sudo dd if=image.wic of=/dev/sdX bs=4M conv=fsync status=progress

# Check SD card health:
sudo fsck /dev/sdX1  # Check boot partition
sudo fsck /dev/sdX2  # Check root partition
```

### Can't Find IP Address

**Solutions:**
```bash
# Use serial console (if available):
screen /dev/ttyUSB0 115200

# Check DHCP server logs
# Try static IP configuration on HDMI console

# Enable SSH over USB (if supported):
# Add dtoverlay=dwc2 to config.txt
```

### SSH Connection Refused

**Solutions:**
```bash
# Check if SSH is enabled:
sudo systemctl enable sshd
sudo systemctl start sshd

# Check firewall:
sudo ufw status
sudo ufw allow ssh

# Reset SSH keys if needed:
sudo ssh-keygen -A
sudo systemctl restart sshd
```

### Robot Services Not Starting

**Check logs:**
```bash
journalctl -u robot-gateway -f
journalctl -u ros2-nodes -f

# Check Python dependencies:
pip3 list | grep -E "(cv2|numpy|ros)"

# Restart services:
sudo systemctl restart robot-gateway
sudo systemctl restart ros2-nodes
```

## 📊 Expected Performance

### Boot Time
- **First boot**: 2-3 minutes (initial setup)
- **Subsequent boots**: 30-60 seconds

### System Resources
- **RAM usage**: ~400-800MB (depends on services)
- **CPU usage**: <20% idle
- **Storage**: ~3-8GB used (depends on packages)

### Network Performance  
- **SSH latency**: <10ms on local network
- **Throughput**: 100Mbps Ethernet
- **WiFi**: Depends on USB adapter (if added)

## ✅ Success Criteria

Your PYNQ-Z2 robot is ready when:

- [ ] SD card boots successfully
- [ ] SSH access works
- [ ] Robot services start automatically
- [ ] Hardware interfaces accessible (`/dev/i2c*`, `/dev/spi*`)
- [ ] Python/OpenCV functional
- [ ] ROS2 nodes can be launched
- [ ] Network connectivity established

## 🎯 Next Steps

Once flashed and booted:

1. **Update system**: `apt update && apt upgrade`
2. **Configure WiFi**: If using wireless
3. **Test sensors**: Connect and test LiDAR, cameras
4. **Deploy robot code**: Upload your control algorithms
5. **Configure startup**: Set services to auto-start

---

**🎉 Congratulations! Your PYNQ-Z2 robot system is now ready for LiDAR SLAM and computer vision applications!**