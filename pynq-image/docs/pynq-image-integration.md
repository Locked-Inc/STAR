# PYNQ Image Integration

Integration guide for deploying C++ applications to your minimal PYNQ-Z2 Ubuntu image.

## Runtime Dependencies Already Satisfied

Your minimal PYNQ image includes all necessary C++ runtime libraries:

✅ **C/C++ Standard Libraries** (already installed):
- `libc6` - GNU C Library (glibc 2.35)
- `libstdc++6` - GNU C++ Standard Library  
- `libgcc-s1` - GCC support library
- `libm.so.6` - Math library
- `libpthread.so.0` - POSIX threads (for std::thread)

✅ **Dynamic Linker**:
- `ld-linux-armhf.so.3` - ARM dynamic linker

## No Additional Packages Needed!

**You do NOT need to add anything to `multistrap.config` for basic C++ programs.**

Your current package list already includes:
```
packages=libstdc++6 libsystemd0
packages=libc-bin libc6
```

These satisfy all C++ runtime requirements.

## Verifying Dependencies

### Before Deploying (on Development Machine)

```bash
# Check what libraries your binary needs
readelf -d build-arm/star_robot | grep NEEDED

# Expected output (example):
 0x00000001 (NEEDED)             Shared library: [libpthread.so.0]
 0x00000001 (NEEDED)             Shared library: [libstdc++.so.6]
 0x00000001 (NEEDED)             Shared library: [libm.so.6]
 0x00000001 (NEEDED)             Shared library: [libgcc_s.so.1]
 0x00000001 (NEEDED)             Shared library: [libc.so.6]
```

All of these are present in your minimal image!

### After Deploying (on PYNQ-Z2)

```bash
# Verify all dependencies are resolved
ldd /home/star/bin/star_robot

# Expected output:
linux-vdso.so.1 (0xbefff000)
libpthread.so.0 => /lib/arm-linux-gnueabihf/libpthread.so.0 (0xb6f20000)
libstdc++.so.6 => /usr/lib/arm-linux-gnueabihf/libstdc++.so.6 (0xb6d80000)
libm.so.6 => /lib/arm-linux-gnueabihf/libm.so.6 (0xb6d10000)
libgcc_s.so.1 => /lib/arm-linux-gnueabihf/libgcc_s.so.1 (0xb6cf0000)
libc.so.6 => /lib/arm-linux-gnueabihf/libc.so.6 (0xb6b90000)
ld-linux-armhf.so.3 (0xb6f50000)
```

If any library shows "not found", that's a problem. But this won't happen with your current image!

## Deployment Methods

### Method 1: scp (Simple File Copy)

```bash
scp build-arm/star_robot star@<pynq-ip>:/home/star/bin/
```

### Method 2: rsync (Incremental Updates)

```bash
# Initial deployment
rsync -avz build-arm/star_robot star@<pynq-ip>:/home/star/bin/

# Subsequent deployments (faster, only transfers changes)
rsync -avz --delete build-arm/ star@<pynq-ip>:/home/star/bin/
```

### Method 3: Systemd Service (Auto-Start on Boot)

Create `/etc/systemd/system/star-robot.service`:

```ini
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

[Install]
WantedBy=multi-user.target
```

**Enable and start:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable star-robot.service
sudo systemctl start star-robot.service
sudo systemctl status star-robot.service

# View logs
sudo journalctl -u star-robot.service -f
```

## Deployment Checklist

- [ ] **Verify binary is ARM**: `file build-arm/star_robot` shows "ARM"
- [ ] **Check dependencies**: `readelf -d build-arm/star_robot | grep NEEDED`
- [ ] **Transfer to target**: `rsync -avz build-arm/star_robot star@<ip>:/home/star/bin/`
- [ ] **Set permissions**: `ssh star@<ip> chmod +x /home/star/bin/star_robot`
- [ ] **Test run**: `ssh star@<ip> /home/star/bin/star_robot --test`
- [ ] **Verify dependencies on target**: `ssh star@<ip> ldd /home/star/bin/star_robot`
- [ ] **Install service** (optional): `sudo cp star-robot.service /etc/systemd/system/`
- [ ] **Enable service**: `sudo systemctl enable star-robot.service`

## Common Issues

### Issue: "Permission denied"

**Solution:**
```bash
ssh star@<pynq-ip> chmod +x /home/star/bin/star_robot
```

### Issue: "No such file or directory" (but file exists)

**Cause:** Wrong architecture or missing dynamic linker

**Solution:**
```bash
# Verify it's an ARM binary
file /home/star/bin/star_robot

# Should show: ELF 32-bit LSB executable, ARM
```

### Issue: Binary runs but crashes immediately

**Solutions:**
1. Check you're not running the x86_64 binary by mistake
2. Verify all dependencies: `ldd /home/star/bin/star_robot`
3. Check for missing shared libraries
4. Run with gdbserver for debugging (see remote-debugging.md)

## Static vs Dynamic Linking Decision

For your PYNQ-Z2 deployment, **use dynamic linking** (default):

| Aspect | Your Situation | Recommendation |
|--------|---------------|----------------|
| Target has standard libs | ✅ Yes (Ubuntu 22.04) | Dynamic |
| Single application | ✅ Yes | Dynamic |
| Need small binary | ✅ Yes (50-200 KB) | Dynamic |
| Frequent updates | ✅ Yes (development) | Dynamic |
| Need portability | ❌ No (fixed target) | Dynamic |

**Only use static linking if you need a single self-contained executable for distribution to multiple embedded systems.**

## Performance Considerations

Your ARM binaries will be optimized for Cortex-A9 with:
- `-march=armv7-a` - ARMv7 instruction set
- `-mcpu=cortex-a9` - Cortex-A9 specific optimizations
- `-mfpu=neon` - NEON SIMD vectorization
- `-mfloat-abi=hard` - Hardware floating-point
- `-mthumb` - Thumb-2 (30% smaller code)
- `-O2` - Level 2 optimization
- Dead code elimination via `--gc-sections`

**Expected performance**: 10-100x faster than compiling on the ARM Cortex-A9 itself!

## Next Steps

- [Build & Deploy Workflow](./build-deploy-workflow.md) - Complete development pipeline
- [Remote Debugging](./remote-debugging.md) - Debug on ARM target
- [SICK TIM561 LiDAR Communication](./lidar-communication.md) - Implement sensor interface
