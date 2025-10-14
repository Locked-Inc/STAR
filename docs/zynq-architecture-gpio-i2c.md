# Zynq Architecture: Understanding FPGA and Linux Integration

## How Does FPGA and Linux Work Together?

## Zynq-7000 Architecture

The Zynq-7000 has **two main parts**:

### 1. **PS (Processing System)** - The ARM Side
- Dual-core ARM Cortex-A9 processor
- This is where **Linux runs**
- Has dedicated peripherals (PS peripherals):
  - Ethernet, USB, UART, SPI, I2C, CAN, GPIO
  - SD card controller
  - These are **hardwired silicon blocks**
  - Controlled directly by Linux device drivers

### 2. **PL (Programmable Logic)** - The FPGA Side
- Traditional FPGA fabric (reconfigurable logic)
- You program this with bitstreams (from Vivado/Vitis)
- Can implement custom hardware blocks
- Can access GPIO pins, implement custom I2C, SPI, etc.

## How PS and PL Interact

They're connected via **AXI buses** (high-speed interconnect):
- PS can read/write to PL registers (memory-mapped I/O)
- PL can access PS memory (DMA)
- Interrupts can flow both directions

```
┌─────────────────────────────────────────────────────┐
│              Zynq-7000 SoC                         │
│                                                     │
│  ┌──────────────────┐      ┌───────────────────┐  │
│  │  PS (ARM Side)   │      │  PL (FPGA Side)   │  │
│  │                  │      │                   │  │
│  │  ┌────────────┐  │      │  ┌─────────────┐ │  │
│  │  │ Cortex-A9  │  │◄────►│  │   Custom    │ │  │
│  │  │   Linux    │  │ AXI  │  │   Logic     │ │  │
│  │  └────────────┘  │      │  └─────────────┘ │  │
│  │                  │      │                   │  │
│  │  PS Peripherals: │      │  PL Blocks:       │  │
│  │  • GPIO (MIO)    │      │  • AXI GPIO       │  │
│  │  • I2C           │      │  • AXI IIC        │  │
│  │  • SPI           │      │  • Custom IP      │  │
│  │  • UART          │      │  • PWM            │  │
│  │  • Ethernet      │      │  • DMA            │  │
│  │  • USB           │      │                   │  │
│  └──────────────────┘      └───────────────────┘  │
│         ↓                           ↓              │
│    MIO Pins                    FPGA I/O Pins       │
└─────────────────────────────────────────────────────┘
```

## GPIO: How to Access It

There are **three ways** to use GPIO pins on Zynq:

### Method 1: **PS GPIO (MIO - Multiplexed I/O)**
**These are pins controlled directly by the ARM processor:**

```bash
# In Linux, use standard GPIO sysfs or libgpiod
echo 54 > /sys/class/gpio/export           # Export GPIO pin 54
echo out > /sys/class/gpio/gpio54/direction # Set as output
echo 1 > /sys/class/gpio/gpio54/value       # Set HIGH
echo 0 > /sys/class/gpio/gpio54/value       # Set LOW
```

Or in Python:
```python
import gpiod

chip = gpiod.Chip('gpiochip0')
line = chip.get_line(54)
line.request(consumer="test", type=gpiod.LINE_REQ_DIR_OUT)
line.set_value(1)  # HIGH
line.set_value(0)  # LOW
```

**Pin mapping:** Check your board's schematic to see which physical pins map to MIO pins.

**MIO GPIO numbering:**
- MIO pins 0-53 are available
- Some are used by peripherals (Ethernet, SD card, etc.)
- Check constraints in your Vivado design

### Method 2: **PS GPIO via EMIO (Extended MIO)**
**PS GPIO that routes through the FPGA fabric:**
- Configured in Vivado when you create the Zynq block design
- Still controlled by Linux like PS GPIO
- The GPIO signals pass through PL but don't use programmable logic
- Extends the number of available GPIO beyond MIO

**In Vivado:**
- Double-click Zynq PS block → MIO Configuration → GPIO
- Enable EMIO GPIO
- Configure number of EMIO pins

**In Linux:**
```bash
# EMIO GPIOs typically start at GPIO 54 (after MIO 0-53)
echo 60 > /sys/class/gpio/export  # EMIO GPIO 6
```

### Method 3: **PL GPIO (Custom FPGA IP)**
**GPIO implemented in programmable logic:**

You create an AXI GPIO IP block in Vivado, then access from Linux:

```python
from pynq import MMIO

# Memory-mapped I/O to PL GPIO block at address 0x41200000
gpio = MMIO(0x41200000, 0x10000)
gpio.write(0x0, 0x1)  # Write to register 0, set pin high
value = gpio.read(0x0)  # Read from register 0
```

**In Vivado:**
1. Add AXI GPIO IP from IP catalog
2. Connect to AXI interconnect
3. Assign base address (e.g., 0x41200000)
4. Connect GPIO output to external pins via constraints

## I2C: How to Access It

### Method 1: **PS I2C (Hardware I2C controller)**
**Built-in I2C controller in the ARM processor:**

The Zynq has dedicated I2C controllers (usually 2). Check your board schematic to see which I2C bus goes to which pins.

```bash
# In Linux, use i2c-tools
i2cdetect -y 0          # Scan I2C bus 0
i2cget -y 0 0x50 0x00   # Read from device at address 0x50, register 0x00
i2cset -y 0 0x50 0x00 0xFF  # Write 0xFF to device 0x50, register 0x00
```

In Python:
```python
from smbus2 import SMBus

bus = SMBus(0)  # I2C bus 0
bus.write_byte_data(0x50, 0x00, 0xFF)  # Write to register
data = bus.read_byte_data(0x50, 0x00)  # Read from register
bus.close()
```

**Which I2C bus to use?**
- Check your board schematic
- Common: I2C0 on MIO pins 10-11, I2C1 on MIO pins 12-13
- Enable in Vivado PS configuration

### Method 2: **PL I2C (Custom FPGA IP)**
**I2C controller implemented in FPGA fabric:**

- Add AXI IIC IP block in Vivado
- Access from Linux via memory-mapped I/O

Or with PYNQ:
```python
from pynq.lib import AxiIIC

iic = AxiIIC(0x41600000)  # Base address of AXI IIC in FPGA
iic.send(0x50, [0x00, 0xFF], 2)  # Send data to device
data = iic.receive(0x50, 1)  # Receive 1 byte
```

## How It Works Together

Here's a typical workflow:

### 1. **Boot Process:**
```
Power On
  ↓
Boot ROM (reads boot mode pins)
  ↓
FSBL loads from BOOT.BIN
  ↓
FPGA bitstream loaded (PL configured)
  ↓
U-Boot loads
  ↓
Linux kernel boots
  ↓
Device tree tells Linux about hardware
  ↓
Drivers initialize PS peripherals
  ↓
Your applications start (can use PS and PL)
```

### 2. **Device Tree Controls Everything:**
```dts
/* Example device tree snippet */
i2c0: i2c@e0004000 {
    compatible = "cdns,i2c-r1p10";
    reg = <0xe0004000 0x1000>;
    interrupts = <0 25 4>;
    clocks = <&clkc 38>;
    status = "okay";

    /* Your I2C devices */
    temp_sensor@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
    };
};

gpio@e000a000 {
    compatible = "xlnx,zynq-gpio-1.0";
    reg = <0xe000a000 0x1000>;
    interrupts = <0 20 4>;
    gpio-controller;
    #gpio-cells = <2>;
};

/* Custom FPGA IP */
axi_gpio_0: gpio@41200000 {
    compatible = "xlnx,xps-gpio-1.00.a";
    reg = <0x41200000 0x10000>;
    xlnx,gpio-width = <8>;
};
```

### 3. **Accessing PL from Linux:**
- Load a new bitstream: `fpga-load-overlay` or PYNQ overlays
- Access FPGA registers via `/dev/mem` or PYNQ's MMIO class
- FPGA blocks show up as memory addresses

## Pin Assignment: The Critical Part

**This is determined in Vivado when you create the hardware design:**

### Step-by-Step Process:

1. **In Vivado, create a block design:**
   - Add Zynq PS block
   - Configure which peripherals to enable

2. **Configure the Zynq PS block:**
   - Double-click the Zynq PS block
   - Go to "MIO Configuration"
   - Enable peripherals (I2C, GPIO, SPI, etc.)
   - Assign them to MIO pins

   Example:
   - I2C 0: MIO 10-11
   - GPIO: MIO 0, 9, 14-15
   - UART 1: MIO 48-49
   - SD 0: MIO 40-45

3. **Create constraints file (`.xdc`):**
   Maps PL signals to physical FPGA pins:
   ```tcl
   # Example constraints
   set_property PACKAGE_PIN Y9  [get_ports {led[0]}]
   set_property IOSTANDARD LVCMOS33 [get_ports {led[0]}]

   set_property PACKAGE_PIN Y11 [get_ports {pwm_out}]
   set_property IOSTANDARD LVCMOS33 [get_ports {pwm_out}]
   ```

4. **Vivado generates:**
   - Bitstream (`.bit` file)
   - Hardware handoff file (`.xsa`)

5. **PetaLinux uses the `.xsa` to generate:**
   - Device tree with proper pin mappings
   - Kernel drivers configured correctly

## Your STAR-Z2 Board Setup

Looking at your setup, **you don't have a BSP specified yet** (`BSP_STAR-Z2 :=`). This means:

- You're using a **default Zynq configuration**
- May not have all peripherals properly configured
- Pin mappings might not match your actual board

### To properly use GPIO/I2C, you need:

1. **Check your board schematic** - see which pins go where
2. **Create or customize a Vivado hardware project** that:
   - Enables the PS peripherals you need
   - Assigns them to the correct MIO pins
   - Creates any PL blocks you need
3. **Export the XSA file** from Vivado
4. **Point your BSP to it** in the `.spec` file:
   ```makefile
   BSP_STAR-Z2 := my_hardware.xsa
   ```
5. **PetaLinux will generate the correct device tree**

## Quick Example Workflow

Let's say you want to use I2C bus 0 on specific pins:

### In Vivado:
1. Create new project or open existing
2. Create block design
3. Add Zynq PS IP
4. Double-click Zynq block → MIO Configuration → I2C
5. Enable I2C 0
6. Assign to MIO pins (e.g., MIO 10 for SCL, MIO 11 for SDA)
7. If using PL GPIO: Add AXI GPIO IP, connect to interconnect
8. Create constraints for any PL pins
9. Generate bitstream
10. Export hardware (File → Export → Export Hardware)

### In Your Board Spec:
```makefile
BSP_STAR-Z2 := my_hardware.xsa
BITSTREAM_STAR-Z2 := my_hardware.bit
```

### After Building, in Linux:
```bash
# I2C should be available
ls /dev/i2c*
# /dev/i2c-0

# Test it
i2cdetect -y 0
#      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
# 00:          -- -- -- -- -- -- -- -- -- -- -- -- --
# 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
# 20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
# 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
# 40: -- -- -- -- -- -- -- -- UU -- -- -- -- -- -- --
# 50: 50 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
# 60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
# 70: -- -- -- -- -- -- -- --
```

## Summary of Access Methods

| Hardware | Location | Access Method | Example |
|----------|----------|---------------|---------|
| **PS GPIO (MIO)** | ARM processor | Linux GPIO driver | `echo 54 > /sys/class/gpio/export` |
| **PS I2C** | ARM processor | Linux I2C driver | `i2cdetect -y 0` |
| **PS SPI** | ARM processor | Linux SPI driver | `/dev/spidev0.0` |
| **PS UART** | ARM processor | Linux TTY driver | `/dev/ttyPS0` |
| **EMIO GPIO** | ARM → FPGA fabric | Linux GPIO driver | `echo 60 > /sys/class/gpio/export` |
| **PL GPIO** | FPGA fabric | Memory-mapped I/O | `MMIO(0x41200000)` |
| **PL Custom IP** | FPGA fabric | Memory-mapped I/O | `MMIO(base_addr)` |

## Key Takeaways

- **PS peripherals** (I2C, GPIO, etc.) are controlled by standard Linux drivers
- **PL peripherals** are custom FPGA blocks accessed via memory-mapped I/O
- **Pin assignment** happens in Vivado during hardware design
- **Device tree** tells Linux about the hardware configuration
- **BSP/XSA file** contains all the hardware information PetaLinux needs
- Without a proper XSA, you're using default pin mappings that may not match your board
