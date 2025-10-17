# Build & Deploy Workflow

Complete development to deployment pipeline for C++ applications on PYNQ-Z2.

## Quick Reference

```bash
# 1. Edit code on development machine
vim src/main.cpp

# 2. Cross-compile
cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-cortex-a9-toolchain.cmake ..
make -j$(nproc)

# 3. Verify binary
file star_robot
readelf -d star_robot | grep NEEDED

# 4. Deploy to PYNQ-Z2
rsync -avz star_robot star@192.168.1.100:/home/star/bin/

# 5. Run on target
ssh star@192.168.1.100
cd /home/star/bin
./star_robot
```

## Development Cycle

### Fast Iteration Workflow

**For rapid development:**

```bash
# Create deployment script: deploy.sh
#!/bin/bash
set -e

PYNQ_IP="192.168.1.100"  # Replace with your PYNQ-Z2 IP
PYNQ_USER="star"
BUILD_DIR="build-arm"
TARGET_DIR="/home/star/bin"

echo "Building for ARM..."
cd $BUILD_DIR
make -j$(nproc)

echo "Verifying binary..."
file star_robot | grep -q "ARM" || { echo "Error: Not an ARM binary!"; exit 1; }

echo "Deploying to PYNQ-Z2..."
rsync -avz --progress star_robot ${PYNQ_USER}@${PYNQ_IP}:${TARGET_DIR}/

echo "Deployment complete!"
echo "Run on target: ssh ${PYNQ_USER}@${PYNQ_IP} ${TARGET_DIR}/star_robot"
```

**Make executable:**
```bash
chmod +x deploy.sh
```

**Usage:**
```bash
./deploy.sh
```

### Build, Deploy, and Run in One Command

```bash
#!/bin/bash
# build-deploy-run.sh
set -e

PYNQ_IP="192.168.1.100"
PYNQ_USER="star"

cd build-arm && make -j$(nproc) && cd ..
rsync -avz build-arm/star_robot ${PYNQ_USER}@${PYNQ_IP}:/home/star/bin/
ssh ${PYNQ_USER}@${PYNQ_IP} "/home/star/bin/star_robot"
```

## Systemd Service Management

### Creating the Service

```ini
# /etc/systemd/system/star-robot.service
[Unit]
Description=STAR Autonomous Robot Controller
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=star
WorkingDirectory=/home/star/bin
ExecStart=/home/star/bin/star_robot
Restart=on-failure
RestartSec=5s
StandardOutput=journal
StandardError=journal

# Resource limits (optional)
MemoryLimit=512M
CPUQuota=80%

[Install]
WantedBy=multi-user.target
```

### Service Management Commands

```bash
# Install service
sudo cp star-robot.service /etc/systemd/system/
sudo systemctl daemon-reload

# Enable (start on boot)
sudo systemctl enable star-robot.service

# Start now
sudo systemctl start star-robot.service

# Check status
sudo systemctl status star-robot.service

# View logs (live)
sudo journalctl -u star-robot.service -f

# View logs (last 100 lines)
sudo journalctl -u star-robot.service -n 100

# Stop service
sudo systemctl stop star-robot.service

# Restart service
sudo systemctl restart star-robot.service

# Disable (don't start on boot)
sudo systemctl disable star-robot.service
```

### Updating the Running Service

```bash
# 1. Build new version
cd build-arm && make -j$(nproc)

# 2. Deploy
rsync -avz star_robot star@192.168.1.100:/home/star/bin/

# 3. Restart service
ssh star@192.168.1.100 sudo systemctl restart star-robot.service

# 4. Verify
ssh star@192.168.1.100 sudo systemctl status star-robot.service
```

## Performance Monitoring

### Check Resource Usage

```bash
# CPU and memory usage
ssh star@192.168.1.100 top -b -n 1 | grep star_robot

# Detailed process info
ssh star@192.168.1.100 ps aux | grep star_robot

# System resource overview
ssh star@192.168.1.100 htop  # If installed
```

### Binary Size Analysis

```bash
# Check binary size
ls -lh build-arm/star_robot

# Stripped vs unstripped
strip build-arm/star_robot -o build-arm/star_robot_stripped
ls -lh build-arm/star_robot*

# Section sizes
size build-arm/star_robot
```

## Continuous Integration (Optional)

### Basic Build Script

```bash
#!/bin/bash
# ci-build.sh
set -e

echo "=== CI Build for STAR Robot ==="

echo "Step 1: Clean build directory..."
rm -rf build-arm
mkdir build-arm

echo "Step 2: Configure CMake..."
cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-cortex-a9-toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      ..

echo "Step 3: Build..."
make -j$(nproc)

echo "Step 4: Verify binary..."
file star_robot | grep -q "ARM" || { echo "ERROR: Not ARM binary!"; exit 1; }

echo "Step 5: Check size..."
ls -lh star_robot

echo "Step 6: Check dependencies..."
readelf -d star_robot | grep NEEDED

echo "=== Build successful! ==="
```

## Deployment Checklist

```bash
# Pre-deployment verification script
#!/bin/bash
# verify-before-deploy.sh

BINARY="build-arm/star_robot"

echo "Verification checklist:"

# 1. Check file exists
if [ ! -f "$BINARY" ]; then
    echo "❌ Binary not found: $BINARY"
    exit 1
fi
echo "✅ Binary exists"

# 2. Check architecture
if file "$BINARY" | grep -q "ARM"; then
    echo "✅ Binary is ARM"
else
    echo "❌ Binary is NOT ARM"
    exit 1
fi

# 3. Check it's executable
if [ -x "$BINARY" ]; then
    echo "✅ Binary is executable"
else
    echo "❌ Binary is not executable"
    exit 1
fi

# 4. Check dependencies
echo "Dependencies:"
readelf -d "$BINARY" | grep NEEDED | awk '{print "  -", $5}'

echo ""
echo "✅ All checks passed! Ready to deploy."
```

## Troubleshooting

### Build Fails

```bash
# Clean and rebuild
rm -rf build-arm
mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/arm-cortex-a9-toolchain.cmake ..
make -j$(nproc)
```

### Deployment Fails (Network Issues)

```bash
# Check connectivity
ping -c 3 <pynq-ip>

# Test SSH
ssh star@<pynq-ip> echo "SSH works"

# Check disk space on target
ssh star@<pynq-ip> df -h
```

### Binary Doesn't Run on Target

```bash
# 1. Verify architecture
ssh star@<pynq-ip> file /home/star/bin/star_robot

# 2. Check dependencies
ssh star@<pynq-ip> ldd /home/star/bin/star_robot

# 3. Check permissions
ssh star@<pynq-ip> ls -l /home/star/bin/star_robot
ssh star@<pynq-ip> chmod +x /home/star/bin/star_robot

# 4. Try running directly
ssh star@<pynq-ip> /home/star/bin/star_robot

# 5. Check for errors
ssh star@<pynq-ip> dmesg | tail -20
```

## Best Practices

1. **Version Control**: Tag releases before deploying to hardware
2. **Backup**: Keep previous working binary before updating
3. **Testing**: Test on host (x86_64) first when possible
4. **Logging**: Use proper logging for debugging on target
5. **Monitoring**: Check system resources after deployment

## Next Steps

- [Remote Debugging](./remote-debugging.md) - Debug on ARM target with GDB
- [Embedded C++ Best Practices](./embedded-cpp-best-practices.md) - Optimization techniques
