# STAR Robot OS

Custom embedded Linux operating system for the **STAR (Simultaneous Tracking And Robotics)** robot - a LiDAR SLAM robot with ML-powered computer vision and remote control capabilities.

## Features

- **Raspberry Pi 5** - BCM2712 SoC with Cortex-A76 CPU
- **ROS2 Jazzy** - Latest ROS2 distribution for robotics
- **OpenJDK 17** - Java runtime for robot gateway application
- **OpenCV 4** - Computer vision with DNN module for ML inference
- **Python ML Stack** - NumPy, SciPy, Pillow for ML applications
- **Dual CSI Camera Support** - Stereo vision capability
- **WiFi & Ethernet** - Full networking support
- **mDNS** - Easy discovery as `star-robot.local`
- **Minimal Footprint** - ~2GB base image, expandable to SD card size

## Hardware Support

### Sensors
- **TiM561 LiDAR** - Ethernet-based SICK LiDAR for SLAM
- **Dual CSI Cameras** - Raspberry Pi Camera Module 3 (stereo setup)
- **USB Cameras** - Alternative camera support via V4L2

### Networking
- **WiFi** - Built-in Pi 5 WiFi with wpa_supplicant
- **Ethernet** - Gigabit Ethernet for LiDAR and development
- **mDNS/Avahi** - Access robot at `star-robot.local`

### Machine Learning
- **YOLO Support** - Person and object detection via OpenCV DNN
- **Visual SLAM** - ROS2 packages for camera-based SLAM
- **CPU Inference** - Optimized for ARM Cortex-A76

## Prerequisites

Build requirements (Ubuntu/Debian):
```bash
sudo apt-get install -y \
    build-essential \
    gcc-aarch64-linux-gnu \
    git \
    wget \
    cpio \
    python3 \
    unzip \
    rsync \
    bc \
    libncurses5-dev \
    libssl-dev \
    dosfstools \
    mtools \
    genimage
```

## Building

1. Clone this repository:
```bash
git clone https://github.com/Locked-Inc/STAR.git
cd STAR/star-pi5-os
```

2. Build using Make:
```bash
make build
```

**Build time:** 1-2 hours on a modern system (first build downloads and compiles everything)

The build process will:
- Download Buildroot 2025.02 LTS
- Configure for Raspberry Pi 5 64-bit (BCM2712, Cortex-A76)
- Build kernel, bootloader, and root filesystem
- Generate a flashable SD card image

## Flashing to SD Card

After successful build:

```bash
# Find your SD card device
lsblk

# Flash the image (REPLACE /dev/sdX with your SD card!)
sudo dd if=buildroot-2025.02/output/images/sdcard.img of=/dev/sdX bs=4M status=progress conv=fsync

# Sync and safely remove
sync
```

**WARNING:** Double-check the device name! `dd` will overwrite the target device.

### Expanding Partition (Optional)

To use full SD card capacity:
```bash
# After first boot, SSH into robot and run:
resize2fs /dev/mmcblk0p2
```

## First Boot & Setup

### 1. Initial Boot
- Insert SD card into Raspberry Pi 5
- Connect ethernet cable (optional, for initial setup)
- Power on
- Wait ~30 seconds for boot

### 2. Network Access

**Via Ethernet:**
```bash
# Find IP from your router, or use nmap:
nmap -sn 192.168.1.0/24 | grep star-robot

# SSH using IP
ssh root@<ip-address>
# Default: no password (set one immediately!)
```

**Via mDNS:**
```bash
# Once mDNS is working:
ssh root@star-robot.local
```

### 3. Initial Configuration

```bash
# Set root password
passwd

# Configure WiFi
/root/setup-wifi.sh

# Verify network connectivity
ping 8.8.8.8
```

### 4. WiFi Configuration

The system includes a helper script for easy WiFi setup:

```bash
# Run the WiFi setup script
/root/setup-wifi.sh

# Or manually edit configuration
nano /etc/wpa_supplicant/wpa_supplicant.conf
```

## Hardware Configuration

### Camera Setup

The OS supports dual CSI cameras out of the box via libcamera library:

```bash
# Test cameras with V4L2
v4l2-ctl --list-devices

# Test camera capture with OpenCV (Python)
python3 << 'EOF'
import cv2
cap = cv2.VideoCapture(0)
if cap.isOpened():
    ret, frame = cap.read()
    if ret:
        cv2.imwrite('/tmp/test.jpg', frame)
        print("Camera 0 OK - saved /tmp/test.jpg")
    cap.release()
else:
    print("Camera 0 Error")
EOF
```

**Note**: CLI tools like `libcamera-hello` are not included due to version compatibility issues. Use V4L2 or OpenCV for camera testing.

### LiDAR (TiM561)

The TiM561 connects via Ethernet:
1. Connect LiDAR to Ethernet port (or USB-Ethernet adapter)
2. Configure static IP: TiM561 default is `192.168.0.1`
3. Set Pi interface to `192.168.0.10`:
```bash
ip addr add 192.168.0.10/24 dev eth0
```

### Testing OpenCV & ML

```bash
# Test OpenCV installation
python3 -c "import cv2; print(cv2.__version__)"

# Test NumPy
python3 -c "import numpy as np; print(np.__version__)"

# Test camera with OpenCV
python3 -c "import cv2; cap = cv2.VideoCapture(0); print('Camera OK' if cap.isOpened() else 'Camera Error')"
```

## Deploying Robot Gateway

The system includes a systemd service for the Java robot-gateway application:

### 1. Build Gateway JAR
```bash
# On your development machine
cd /path/to/STAR/robot-gateway
./gradlew bootJar
```

### 2. Deploy to Robot
```bash
# Copy JAR to robot
scp build/libs/robot-gateway.jar root@star-robot.local:/opt/robot-gateway/

# Enable service
ssh root@star-robot.local
systemctl enable robot-gateway
systemctl start robot-gateway
```

### 3. Monitor Service
```bash
# Check status
systemctl status robot-gateway

# View logs
journalctl -u robot-gateway -f

# Restart after updates
systemctl restart robot-gateway
```

The service auto-starts on boot and restarts on failure.

## ROS2 Usage

ROS2 environment is automatically sourced on login:

```bash
# Verify ROS2
ros2 --version

# List topics
ros2 topic list

# Launch SLAM (example)
ros2 launch slam_toolbox online_async_launch.py
```

## Installing TensorFlow Lite (Optional)

TensorFlow Lite is not included by default. To install for YOLO inference:

```bash
# Install TensorFlow Lite runtime
pip3 install tflite-runtime

# Or install full TensorFlow (larger)
pip3 install tensorflow
```

## Customization

### Add Packages
Edit `configs/rpi5_64_defconfig` and add packages:
```
BR2_PACKAGE_<PACKAGE_NAME>=y
```

Then rebuild:
```bash
cd buildroot-2025.02
make
```

### Kernel Configuration
```bash
cd buildroot-2025.02
make linux-menuconfig
```

### BusyBox Configuration
```bash
make busybox-menuconfig
```

## Troubleshooting

### Build Fails
- Ensure all prerequisites are installed
- Check disk space (need ~20GB free)
- For WSL users: PATH is automatically cleaned by Makefile
- Check build logs in `buildroot-2025.02/output/build/`

### Boot Issues
- Verify SD card is properly flashed
- Check serial console output (UART on GPIO pins)
- Ensure using Raspberry Pi 5 (not Pi 4 or earlier)
- Verify SD card is not corrupted

### WiFi Not Working
- Run `/root/setup-wifi.sh` for guided setup
- Check WiFi credentials in `/etc/wpa_supplicant/wpa_supplicant.conf`
- Verify country code is correct
- Check WiFi interface: `ip link show wlan0`

### Camera Issues
- Verify camera cables are properly connected
- Check camera detection: `v4l2-ctl --list-devices`
- For CSI cameras, ensure both ports are enabled in `config.txt`
- Test with OpenCV: See camera testing example in "Camera Setup" section
- For USB cameras, verify USB connection and check with `lsusb`

### LiDAR Not Detected
- Verify Ethernet connection
- Check IP configuration: `ip addr show`
- Ping LiDAR: `ping 192.168.0.1`
- Check firewall rules if any

### Robot Gateway Won't Start
- Check JAR file exists: `ls -la /opt/robot-gateway/`
- View service logs: `journalctl -u robot-gateway -n 50`
- Verify Java installation: `java -version`
- Check network connectivity

## Security Notes

**IMPORTANT:** This is configured for development. For production:

1. **Set root password immediately:**
   ```bash
   passwd
   ```

2. **Create non-root user:**
   ```bash
   adduser robotuser
   ```

3. **Disable root SSH login:**
   ```bash
   nano /etc/ssh/sshd_config
   # Set: PermitRootLogin no
   systemctl restart sshd
   ```

4. **Configure firewall:**
   ```bash
   # Install iptables and configure rules
   ```

5. **Secure WiFi:**
   ```bash
   chmod 600 /etc/wpa_supplicant/wpa_supplicant.conf
   ```

## STAR Robot Architecture

```
┌─────────────────────────────────────┐
│   Retroid Pocket 2S Controller      │
│   (Android App)                     │
└───────────────┬─────────────────────┘
                │ WiFi Commands
                ↓
┌─────────────────────────────────────┐
│   Robot Gateway (Spring Boot)       │
│   Port: 8080                        │
└───────────────┬─────────────────────┘
                │ ROS2 Bridge
                ↓
┌─────────────────────────────────────┐
│   ROS2 Jazzy (SLAM & Vision)        │
│   - SLAM Toolbox / Cartographer     │
│   - Camera Pipeline                 │
│   - ML Object Detection             │
└─────┬───────────────────────┬───────┘
      │                       │
      ↓                       ↓
┌──────────────┐      ┌──────────────┐
│  TiM561      │      │  Dual CSI    │
│  LiDAR       │      │  Cameras     │
│  (Ethernet)  │      │  (Stereo)    │
└──────────────┘      └──────────────┘
```

## Resources

- [STAR Robot Project](https://github.com/Locked-Inc/STAR)
- [Buildroot Documentation](https://buildroot.org/docs.html)
- [ROS2 Documentation](https://docs.ros.org/en/jazzy/)
- [Raspberry Pi 5 Documentation](https://www.raspberrypi.com/documentation/)
- [OpenCV Documentation](https://docs.opencv.org/)
- [SICK TiM561 Manual](https://www.sick.com/)

## License

This project configuration is provided as-is. Individual components (Linux kernel, Buildroot, ROS2, OpenJDK, OpenCV) have their own licenses.

---

**Built for STAR Robot Project**
Simultaneous Tracking And Robotics - LiDAR SLAM with ML Computer Vision
