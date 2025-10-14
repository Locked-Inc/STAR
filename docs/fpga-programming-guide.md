# FPGA Programming Guide for Zynq-7000

## FPGA Configuration: Volatile vs. Persistent

### The FPGA Fabric Itself is **NOT Flash Memory**

The FPGA (PL) is **SRAM-based** (volatile):
- ❌ **NOT persistent** - loses configuration when power is removed
- ⚡ **Must be reconfigured on every power-up**
- This is true for ALL Xilinx FPGAs

### Where the Bitstream is Actually Stored (Persistent)

The bitstream **IS stored persistently** in one of these places:

1. **SD Card** (your case - most common for Zynq)
   - Bitstream stored as a `.bit` or `.bin` file
   - Loaded from SD card during boot
   - You can update it by replacing the file

2. **QSPI Flash** (onboard flash memory chip)
   - Some boards have dedicated flash
   - Bitstream stored in QSPI flash chip
   - Loaded from flash during boot

3. **eMMC** (embedded flash storage)
   - Similar to SD card but soldered on

## Your Workflow: SystemVerilog → Bitstream → Board

### Step 1: **Design & Compile on Laptop** (Vivado/Vitis)

```bash
# On your laptop (must have Vivado installed)
vivado

# In Vivado:
# 1. Write SystemVerilog/Verilog/VHDL
# 2. Create block design (Zynq PS + your custom PL logic)
# 3. Create constraints file (.xdc) - pin assignments
# 4. Synthesize → Implement → Generate Bitstream
# 5. Export hardware (.xsa file) and bitstream (.bit file)
```

**Output files:**
- `my_design.bit` - FPGA bitstream
- `my_design.xsa` - Hardware handoff (includes bitstream + metadata)

### Step 2: **Get Bitstream to the Board**

You have **two approaches**:

#### **Approach A: Bake Into Boot Image** (Persistent, loads early)

This is what your current build does:

```makefile
# In STAR-Z2.spec
BITSTREAM_STAR-Z2 := my_design.bit
```

**What happens:**
1. Bitstream gets packaged into `BOOT.BIN`
2. `BOOT.BIN` is stored on SD card boot partition
3. During boot sequence:
   - ✅ U-Boot loads (bootloader)
   - ✅ **FPGA is configured** (your bitstream loaded)
   - ✅ Linux kernel boots
   - ✅ Your FPGA design is ready before Linux even starts

**Persistent?** ✅ YES - bitstream is in `BOOT.BIN` on SD card

**To update:**
```bash
# Rebuild image with new bitstream
make BOARDS=STAR-Z2

# Or manually replace BOOT.BIN on SD card boot partition
```

#### **Approach B: Load from Linux** (Persistent storage, loaded at runtime)

Store bitstream on SD card, load it when needed:

```bash
# Copy bitstream to board
scp my_design.bit xilinx@board:~/overlays/

# On board - load FPGA:
python3
>>> from pynq import Overlay
>>> overlay = Overlay('/home/xilinx/overlays/my_design.bit')
>>> # FPGA is now configured!
```

**Persistent?** ✅ YES - `.bit` file is on SD card (survives reboot)
**But:** FPGA needs to be reconfigured after each reboot (run the Python script again)

**To auto-load on boot:**
```bash
# Create systemd service
sudo nano /etc/systemd/system/fpga-config.service
```

```ini
[Unit]
Description=Configure FPGA on boot
After=multi-user.target

[Service]
Type=oneshot
User=xilinx
ExecStart=/usr/bin/python3 /home/xilinx/load_overlay.py
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable fpga-config.service
```

## What "Flashing" Actually Means

**"Flashing"** typically refers to writing to **non-volatile memory**:

### You ARE "flashing" the SD card:
```bash
# Writing the entire image to SD card (includes BOOT.BIN with bitstream)
sudo dd if=STAR-Z2-3.0.1.img of=/dev/sdb bs=4M status=progress
```
This writes everything including the bitstream to SD card.

### You are NOT directly "flashing" the FPGA:
- FPGA is reconfigured from SD card on each boot
- It's more like "loading" than "flashing"
- The bitstream lives on SD card (persistent)
- FPGA configuration is volatile (temporary)

## Detailed Boot Sequence

Here's what actually happens when you power on:

```
1. Power On
   ↓
2. Boot ROM (hardwired in Zynq chip)
   - Reads boot mode pins
   - Determines to boot from SD card
   ↓
3. First Stage Boot Loader (FSBL) from BOOT.BIN
   - Initializes PS (ARM processors)
   - Loads your bitstream into FPGA
   - ✅ FPGA is now configured!
   ↓
4. U-Boot (bootloader) from BOOT.BIN
   - Loads Linux kernel
   ↓
5. Linux Kernel boots
   - FPGA already configured and ready to use
   ↓
6. Init system (systemd)
   - Starts services
   - Your ROS nodes can now use the FPGA
```

**All of this is stored on the SD card persistently!**

## BOOT.BIN Structure

```
BOOT.BIN contains:
├── FSBL (First Stage Boot Loader)
├── Your FPGA bitstream (my_design.bit)  ← Your SystemVerilog design!
├── U-Boot
└── (optionally) ARM Trusted Firmware
```

Created by PetaLinux `bootgen` tool.

## How to Update Your FPGA Design

### Method 1: **Full Image Rebuild** (cleanest)
```bash
# On laptop:
cd ~/STAR/pynq-image
# Replace bitstream in board directory
cp ~/vivado_projects/my_design/my_design.bit board-config/STAR-Z2/
# Update .spec if needed
make BOARDS=STAR-Z2
# Flash new image to SD card
sudo dd if=output/STAR-Z2-3.0.1.img of=/dev/sdb bs=4M
```

### Method 2: **Replace BOOT.BIN Only** (faster)
```bash
# Mount SD card boot partition
sudo mount /dev/sdb1 /mnt

# Create new BOOT.BIN with your bitstream
# (using bootgen on laptop with PetaLinux)
bootgen -image boot.bif -arch zynq -o BOOT.BIN -w

# Replace on SD card
sudo cp BOOT.BIN /mnt/
sudo umount /mnt
```

### Method 3: **Dynamic Loading** (fastest for development)
```bash
# Just copy new .bit file to running board
scp my_design.bit xilinx@board:~/overlays/

# On board, reload:
python3
>>> from pynq import Overlay
>>> ol = Overlay('my_design.bit')  # Reconfigures FPGA immediately
```

## Example: Complete FPGA Update Cycle

**Scenario:** You designed a custom PWM controller in SystemVerilog

```verilog
// my_pwm.sv
module pwm_controller (
    input wire clk,
    input wire [31:0] duty_cycle,
    output reg pwm_out
);
    // Your PWM logic here
endmodule
```

**Step-by-step:**

1. **Design in Vivado (laptop):**
   - Add your SystemVerilog module
   - Connect to AXI interface
   - Connect to physical pins (via .xdc constraints)
   - Generate bitstream

2. **Export:**
   - File → Export → Export Hardware
   - Generates `my_pwm_design.xsa`
   - Bitstream is `my_pwm_design.bit`

3. **Option A - Bake into image:**
   ```bash
   cp my_pwm_design.bit ~/STAR/pynq-image/board-config/STAR-Z2/
   cd ~/STAR/pynq-image
   make BOARDS=STAR-Z2
   # Flash to SD card
   ```

4. **Option B - Copy to running board:**
   ```bash
   scp my_pwm_design.bit xilinx@192.168.1.100:~/
   ssh xilinx@192.168.1.100
   python3 -c "from pynq import Overlay; ol = Overlay('my_pwm_design.bit')"
   ```

5. **Use from ROS:**
   ```python
   from pynq import MMIO

   # Access your PWM registers
   pwm = MMIO(0x43C00000, 0x10000)  # Your IP's base address
   pwm.write(0x0, 50)  # Set 50% duty cycle
   ```

## Summary Table

| Storage Location | Persistent? | When Loaded? | How to Update? |
|-----------------|-------------|--------------|----------------|
| **SD Card (BOOT.BIN)** | ✅ YES | Every boot (automatic) | Rebuild image or replace BOOT.BIN |
| **SD Card (.bit file)** | ✅ YES | When you run Overlay() | Just copy new .bit file |
| **FPGA Configuration (SRAM)** | ❌ NO | Lost on power-off | N/A - reloads from SD |
| **QSPI Flash** | ✅ YES | Every boot (if configured) | Special flashing tool |

## Key Takeaways

- ✅ Your bitstream IS stored persistently (on SD card)
- ❌ The FPGA itself is NOT persistent (volatile SRAM)
- 🔄 Bitstream automatically reloads from SD card on every boot
- You write SystemVerilog on laptop, compile to bitstream, then copy to SD card (either in BOOT.BIN or as separate .bit file)
