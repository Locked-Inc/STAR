# PYNQ-Z2 Setup Guide

## Board Overview

The **PYNQ-Z2** features a **Xilinx Zynq-7020 SoC** (System on Chip) which combines:
- ARM Cortex-A9 dual-core processor (for running Linux and general computing)
- Xilinx FPGA fabric (for hardware acceleration and custom logic)

This board is used as the main controller for the STAR robot, running a custom Linux image that handles LiDAR SLAM and computer vision tasks.

## Prerequisites

- PYNQ-Z2 board
- MicroSD card (8GB or larger, class 10 recommended)
- MicroSD card reader/writer
- Computer with internet access
- Micro USB cable
- (Optional) Ethernet cable

## Step 1: Download Official PYNQ Image

The latest official PYNQ image is **v3.1.1**.

1. Visit the [PYNQ Supported Boards page](http://www.pynq.io/boards.html)
2. Find the PYNQ-Z2 row
3. Click on the "v3.1.1" link in the "SD card image" column to download
4. Save the `pynq_z2_v3.1.1.zip` file to your project directory

## Step 2: Identify MicroSD Card

1. Insert your microSD card into the card reader
2. Connect the card reader to your computer
3. Run the following command to identify the device:

```bash
lsblk -o NAME,SIZE,TYPE,MOUNTPOINTS,MODEL
```

4. Look for your microSD card device (typically `/dev/sdb` or `/dev/sda`)
   - Verify the size matches your card (e.g., 28.9G for a 32GB card)
   - Note the device name for the next steps

Example output:
```
sdb          28.9G disk /media/user/... MassStorageClass
```

**⚠️ WARNING**: Make absolutely sure you have identified the correct device! Flashing will ERASE ALL DATA on the target device.

## Step 3: Extract and Flash the Image

### Extract the Image

```bash
unzip pynq_z2_v3.1.1.zip
```

This will extract `pynq_z2_v3.1.1.img` (approximately 7.8GB).

### Unmount the MicroSD Card

```bash
umount /dev/sdb  # Replace 'sdb' with your actual device
```

### Flash the Image

```bash
sudo dd if=pynq_z2_v3.1.1.img of=/dev/sdb bs=4M status=progress && sync
```

Replace `/dev/sdb` with your actual device name.

**Notes**:
- This process takes approximately 5-10 minutes depending on your card reader speed
- The `status=progress` flag shows real-time progress
- The `sync` command ensures all data is written to the card before completion
- Expected write speed: 15-50 MB/s

Example output:
```
8334082048 bytes (8.3 GB, 7.8 GiB) copied, 176 s, 47.3 MB/s
1987+1 records in
1987+1 records out
8337309696 bytes (8.3 GB, 7.8 GiB) copied, 437.086 s, 19.1 MB/s
```

## Step 4: Verify the Flash

Check that the partitions were created correctly:

```bash
lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL /dev/sdb
```

You should see two partitions:
```
NAME    SIZE TYPE FSTYPE LABEL
sdb    28.9G disk
├─sdb1  130M part vfat   PYNQ
└─sdb2  7.6G part ext4   root
```

If you see these partitions, the flash was successful!

## Step 5: Boot the PYNQ-Z2 Board

### Hardware Setup

1. **Eject and insert the microSD card**:
   - Safely eject the microSD card from your computer
   - Insert it into the microSD slot on the **underside** of the PYNQ-Z2 board

2. **Configure jumpers**:
   - **JP4 (Boot)**: Set to **SD** position (jumper over top two pins)
   - **JP5 (Power)**:
     - Set to **USB** for USB power (5V via micro USB)
     - Set to **REG** for external 12V power supply

3. **Connect cables**:
   - Connect micro USB cable to **J14 (PROG-UART)** port on the board
   - Connect the other end to your computer
   - (Optional) Connect Ethernet cable for network connectivity

4. **Power on**:
   - Slide the power switch to **ON**

### Boot Process

1. The **Power LED** will light up immediately
2. Watch for the **DONE LED** to illuminate
3. After 30-60 seconds, blue and yellow/green LEDs will light up and stabilize
4. The board is now ready for use

## Step 6: Access PYNQ

### Via Web Interface (Jupyter Notebook)

1. Open a web browser on your computer
2. Navigate to: `http://192.168.2.99`
3. When prompted for credentials:
   - **Username**: `xilinx`
   - **Password**: `xilinx`

### Via SSH (Recommended for Linux Access)

To access the Linux command line environment, SSH is the easiest method:

```bash
ssh xilinx@192.168.2.99
```

Default password: `xilinx`

This gives you full access to the Linux terminal where you can:
- Install packages with `apt`
- Run command-line tools
- Configure system settings
- Develop and test software directly on the board

**Alternative terminal access methods:**
- **Jupyter Terminal**: In the Jupyter interface, click "New" → "Terminal"
- **Serial Console**: Use `sudo screen /dev/ttyUSB1 115200` from your computer

### Network Configuration

The PYNQ-Z2 supports multiple network connection methods:

#### Method 1: USB Network (Default)
By default, the PYNQ-Z2 creates a USB network interface at `192.168.2.99` when connected via USB.

**Note**: This method may not work on all systems. If the USB network interface doesn't appear, use one of the Ethernet methods below.

#### Method 2: Ethernet via Router (DHCP)
Connect the board's Ethernet port to your router/switch. The board will obtain an IP address via DHCP. Check your router's DHCP client list to find the assigned IP address.

#### Method 3: Direct Ethernet Connection (Manual IP)
If the USB network doesn't work, you can connect the board directly to your laptop's Ethernet port:

1. **Connect**: Ethernet cable from PYNQ-Z2 → your laptop's Ethernet port
2. **Configure your laptop's Ethernet interface**:
   - IP: `192.168.2.1`
   - Netmask: `255.255.255.0`

3. **Access the board**:
   - The board uses its default IP: `192.168.2.99`
   - SSH: `ssh xilinx@192.168.2.99`
   - Jupyter: `http://192.168.2.99`

**Important Notes**:
- This only provides laptop ↔ board communication
- The board will **not** have internet access with this method
- To give the board internet access, use Method 2 (router) or share your laptop's internet connection

## Troubleshooting

### Card Not Detected
- Ensure the microSD card is fully inserted into the reader
- Try a different card reader or USB port
- Check that the card is not write-protected

### Boot Fails
- Verify JP4 is set to SD boot mode
- Re-flash the microSD card
- Try a different microSD card (ensure it's class 10 or higher)
- Check power supply (USB port may not provide enough power; try external 12V supply)

### Cannot Access Web Interface
- Verify the board has fully booted (check LEDs)
- Check that your computer recognizes the USB network interface
- Try pinging: `ping 192.168.2.99`
- Check firewall settings

## Next Steps

Once your PYNQ-Z2 is booted and accessible:

1. Explore the Jupyter notebook examples
2. Update the system: `sudo apt update && sudo apt upgrade`
3. Begin integrating with the STAR robot project
4. For custom image building, see `pynq-image-build/README.md`

## References

- [PYNQ Documentation](https://pynq.readthedocs.io/)
- [PYNQ Boards Page](http://www.pynq.io/boards.html)
- [PYNQ Community Forum](https://discuss.pynq.io/)
- [PYNQ-Z2 User Manual](https://www.mouser.com/datasheet/2/744/pynqz2_user_manual_v1_0-1525725.pdf)
