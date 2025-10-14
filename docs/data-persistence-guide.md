# Data Persistence and Development Workflow on Zynq

## What Resets When?

### FPGA (PL) - **Volatile, Resets:**
- ✅ **FPGA configuration (bitstream) is LOST on power cycle**
- ✅ **All FPGA registers/state is LOST**
- But: Bitstream is **reloaded from SD card on boot** (if configured)
- Or: Can be reloaded dynamically from Linux using PYNQ overlays

### Linux Filesystem (SD Card) - **Persistent:**
- ❌ **NOT lost on reboot**
- ✅ **All files on SD card persist** (it's just a normal filesystem)
- ✅ **Your scripts, ROS packages, data** - all saved
- ✅ **systemd services, configuration files** - all persist

### RAM - **Volatile:**
- Programs running in memory are lost on reboot
- But they restart from SD card on boot

## Development Workflow: Two Approaches

### Approach 1: **Develop Directly on the Board** (Recommended for Software)

**Yes, you absolutely CAN develop on the board itself!**

```bash
# SSH into your board
ssh xilinx@192.168.1.100

# Edit files directly
cd ~/my_robot_code
vim motor_controller.py

# Save to SD card - it persists!
# Reboot - your changes are still there

# Install packages - they persist
sudo apt install some-package

# Create systemd service - it persists
sudo systemctl enable my-robot.service
```

**What persists on SD card:**
- `/home/xilinx/` - your home directory, all your code
- `/opt/` - where ROS is installed
- `/etc/` - configuration files, systemd services
- `/usr/local/` - custom installed software
- Everything except `/tmp/` and `/var/run/`

**This is the normal way to work!** Just like developing on a Raspberry Pi or any Linux computer.

### Approach 2: **Build Everything Into the Image** (For Production/Deployment)

This is what you're doing with the PYNQ image build - "baking in" everything before deployment.

**Use this for:**
- Base system packages
- ROS installation
- System configuration
- Default overlays/bitstreams

**Workflow:**
1. Develop/test on the board
2. Once stable, add to image build
3. Rebuild image for clean deployments

## Typical Development Cycle

### For Software (ROS nodes, Python scripts, C++ programs):

```bash
# Method 1: Edit directly on board via SSH
ssh xilinx@board
cd ~/catkin_ws/src/my_package
vim src/my_node.cpp
catkin_make  # or colcon build
# Changes saved to SD card automatically

# Method 2: Edit on laptop, copy to board
# On laptop:
vim my_node.cpp
scp my_node.cpp xilinx@board:~/catkin_ws/src/my_package/src/

# Method 3: Git workflow (best for team/production)
# On laptop:
git commit -am "Fixed motor controller"
git push

# On board:
cd ~/my_robot
git pull
catkin_make
```

### For FPGA (bitstreams):

**FPGA designs MUST be compiled on your dev laptop** (Vivado/Vitis):
```bash
# On laptop with Vivado:
vivado my_project.xpr
# Make changes
# Generate bitstream → my_design.bit

# Copy to board:
scp my_design.bit xilinx@board:~/overlays/

# On board:
cd ~/overlays
# Load new bitstream:
python3 -c "from pynq import Overlay; ol = Overlay('my_design.bit')"
# Or configure boot to load it automatically
```

## Bitstream Boot Configuration

To load a bitstream automatically on boot, you have options:

### Option 1: **Load in BOOT.BIN** (earliest, during U-Boot)
- Bitstream baked into BOOT.BIN
- Loads before Linux boots
- This is what your build does with `BITSTREAM_STAR-Z2`

### Option 2: **Load from Linux** (after boot)
```bash
# Create a systemd service
sudo nano /etc/systemd/system/load-fpga.service
```

```ini
[Unit]
Description=Load FPGA bitstream
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/load_bitstream.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

```bash
# Script to load bitstream
sudo nano /usr/local/bin/load_bitstream.sh
```

```bash
#!/bin/bash
echo "Loading FPGA overlay..."
python3 -c "from pynq import Overlay; Overlay('/home/xilinx/my_design.bit')"
```

```bash
sudo chmod +x /usr/local/bin/load_bitstream.sh
sudo systemctl enable load-fpga.service
```

**This all persists on SD card!**

## Example: Setting Up ROS Service That Persists

```bash
# On the board (via SSH)
sudo nano /etc/systemd/system/robot.service
```

```ini
[Unit]
Description=STAR Robot Control System
After=network.target

[Service]
Type=simple
User=xilinx
WorkingDirectory=/home/xilinx/robot_ws
Environment="ROS_HOME=/home/xilinx/.ros"
ExecStart=/bin/bash -c "source /opt/ros/humble/setup.bash && source /home/xilinx/robot_ws/install/setup.bash && ros2 launch star_robot robot.launch.py"
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable robot.service
sudo systemctl start robot.service

# Check status
sudo systemctl status robot.service

# Reboot and it starts automatically!
sudo reboot
```

## Data Logging / Persistence

```bash
# Save sensor data to SD card
cd /home/xilinx/data
rosbag record -a  # Records all topics to SD card

# Save configuration
nano ~/robot_config.yaml  # Persists on SD card

# Save maps (for SLAM)
ros2 run nav2_map_server map_saver_cli -f ~/maps/my_map
# Saved to SD card permanently
```

## SD Card Space Management

Your SD card is probably 16GB or 32GB. Check usage:
```bash
df -h
# Filesystem      Size  Used Avail Use% Mounted on
# /dev/mmcblk0p2   30G   8G   20G  29% /
```

For data-heavy applications (lidar logs), you might want:
- Larger SD card
- USB drive mounted for data storage
- Network storage (NFS/SMB to laptop)

## Recommended Development Workflow

**For your robot project:**

1. **Initial Setup (one time):**
   - Build and flash PYNQ image to SD card
   - Boot board, configure network
   - Install additional packages (saved to SD card)

2. **Daily Development:**
   - SSH into board
   - Edit code directly or sync from laptop
   - Test immediately
   - systemctl restart services
   - Everything persists on SD card

3. **FPGA Changes:**
   - Compile bitstream on laptop (Vivado)
   - SCP to board
   - Test with PYNQ overlay
   - Once stable, add to image build for clean deployment

4. **Production/Clean Deployment:**
   - Rebuild image with all changes baked in
   - Flash to SD card for clean install

## Summary

| What | Volatile? | Persists? |
|------|-----------|-----------|
| FPGA bitstream (in PL registers) | ✅ YES | ❌ NO - reloaded on boot |
| Files on SD card | ❌ NO | ✅ YES - normal filesystem |
| systemd services | ❌ NO | ✅ YES - files on SD card |
| ROS packages you install | ❌ NO | ✅ YES - saved to SD card |
| Python scripts | ❌ NO | ✅ YES - saved to SD card |
| Running processes in RAM | ✅ YES | ❌ NO - restart from SD on boot |
| Lidar data you save | ❌ NO | ✅ YES - if saved to SD card |

**Bottom line: Treat it like any Linux computer. The SD card is your hard drive. Everything you save there persists. Only FPGA configuration and running programs are lost on reboot (but FPGA reloads automatically).**
