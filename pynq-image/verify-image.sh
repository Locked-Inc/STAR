#!/bin/bash
# STAR Image Verification Script
# Run this on the PYNQ-Z2 board after booting

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STAR Image Verification${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to check command
check_command() {
    local name=$1
    local command=$2
    local expected=$3

    echo -e "${YELLOW}Checking $name...${NC}"
    if result=$(eval "$command" 2>&1); then
        if [ -n "$expected" ]; then
            if echo "$result" | grep -q "$expected"; then
                echo -e "  ${GREEN}✓${NC} $result"
            else
                echo -e "  ${RED}✗${NC} Expected: $expected, Got: $result"
            fi
        else
            echo -e "  ${GREEN}✓${NC} $result"
        fi
    else
        echo -e "  ${RED}✗${NC} Command failed: $command"
    fi
    echo ""
}

# Check user
echo -e "${YELLOW}Current User:${NC}"
whoami
echo ""

# Check Java version (should be 17)
check_command "Java version" "java -version 2>&1 | head -1" "17"

# Check Jupyter (should NOT be installed)
echo -e "${YELLOW}Checking Jupyter (should fail)...${NC}"
if command -v jupyter &> /dev/null; then
    echo -e "  ${RED}✗${NC} Jupyter is installed (should be removed!)"
else
    echo -e "  ${GREEN}✓${NC} Jupyter not found (correct)"
fi
echo ""

# Check gcc-mb (should NOT be installed)
echo -e "${YELLOW}Checking gcc-mb (should fail)...${NC}"
if command -v microblaze-xilinx-elf-gcc &> /dev/null; then
    echo -e "  ${RED}✗${NC} gcc-mb is installed (should be removed!)"
else
    echo -e "  ${GREEN}✓${NC} gcc-mb not found (correct)"
fi
echo ""

# Check Samba (should NOT be installed)
echo -e "${YELLOW}Checking Samba (should fail)...${NC}"
if command -v smbd &> /dev/null; then
    echo -e "  ${RED}✗${NC} Samba is installed (should be removed!)"
else
    echo -e "  ${GREEN}✓${NC} Samba not found (correct)"
fi
echo ""

# Check Python 2.7 (should NOT be installed)
echo -e "${YELLOW}Checking Python 2.7 (should fail)...${NC}"
if command -v python2.7 &> /dev/null; then
    echo -e "  ${RED}✗${NC} Python 2.7 is installed (should be removed!)"
else
    echo -e "  ${GREEN}✓${NC} Python 2.7 not found (correct)"
fi
echo ""

# Check X11 (should NOT be installed)
echo -e "${YELLOW}Checking X11 (should fail)...${NC}"
if command -v startx &> /dev/null; then
    echo -e "  ${RED}✗${NC} X11 is installed (should be removed!)"
else
    echo -e "  ${GREEN}✓${NC} X11 not found (correct)"
fi
echo ""

# Check Bluetooth (should NOT be installed)
echo -e "${YELLOW}Checking Bluetooth (should fail)...${NC}"
if command -v bluetoothctl &> /dev/null; then
    echo -e "  ${RED}✗${NC} Bluetooth is installed (should be removed!)"
else
    echo -e "  ${GREEN}✓${NC} Bluetooth not found (correct)"
fi
echo ""

# Check I2C devices
check_command "I2C devices" "ls /dev/i2c-* 2>&1 | wc -l" ""

# Check USB devices
echo -e "${YELLOW}USB devices:${NC}"
lsusb
echo ""

# Check UART devices
echo -e "${YELLOW}UART devices:${NC}"
ls -l /dev/ttyPS* /dev/ttyUSB* 2>/dev/null || echo "  No UART devices found"
echo ""

# Check UART1 (should be /dev/ttyPS1 for ESP32)
echo -e "${YELLOW}Checking UART1 for ESP32...${NC}"
if [ -e /dev/ttyPS1 ]; then
    echo -e "  ${GREEN}✓${NC} /dev/ttyPS1 exists"
else
    echo -e "  ${RED}✗${NC} /dev/ttyPS1 NOT found (UART1 not enabled)"
fi
echo ""

# Check disk usage
echo -e "${YELLOW}Disk usage:${NC}"
df -h / | grep -v Filesystem
echo ""

# Check memory
echo -e "${YELLOW}Memory usage:${NC}"
free -h | grep -E "Mem:|Swap:"
echo ""

# Check groups
echo -e "${YELLOW}User groups:${NC}"
groups
echo ""

# Check PYNQ version
echo -e "${YELLOW}PYNQ version:${NC}"
python3 -c "import pynq; print(f'PYNQ {pynq.__version__}')" 2>/dev/null || echo "  PYNQ not found"
echo ""

# Check network
echo -e "${YELLOW}Network configuration:${NC}"
ip addr show eth0 2>/dev/null | grep inet || echo "  No eth0 interface"
echo ""

# System info
echo -e "${YELLOW}System info:${NC}"
uname -a
echo ""

# Check partition labels
echo -e "${YELLOW}Partition labels:${NC}"
lsblk -o NAME,LABEL,SIZE,FSTYPE /dev/mmcblk0 2>/dev/null || lsblk
echo ""

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Verification Complete${NC}"
echo -e "${BLUE}========================================${NC}"
