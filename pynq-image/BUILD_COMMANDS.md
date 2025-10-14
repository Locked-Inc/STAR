# Commands to Complete STAR-Z2 Image Build

## Current Situation
- ✅ Stage 1 & 2 rootfs built (saved hours!)
- ✅ Prebuilt PYNQ sdist exists: `PYNQ/sdbuild/prebuilt/pynq_sdist.tar.gz`
- ❌ Missing prebuilt ARM rootfs (expected at: `PYNQ/sdbuild/prebuilt/pynq_rootfs.arm.tar.gz`)

## Solution: Use Your Already-Built Stage 2 as the Prebuilt Rootfs

### Step 1: Create symlink to use your Stage 2 build as prebuilt rootfs
```bash
cd ~/Documents/git/STAR/pynq-image/PYNQ/sdbuild/prebuilt
ln -sf ../build/jammy.arm.stage2.tar.gz pynq_rootfs.arm.tar.gz
```

This tells the build system to use the rootfs you already spent hours building!

### Step 2: Run the build (should complete in 20-40 minutes)
```bash
cd ~/Documents/git/STAR/pynq-image
make BOARDS=STAR-Z2
```

## What This Will Do
1. ✅ Skip rebuilding Stage 1 & 2 (uses your prebuilt via symlink)
2. ✅ Use prebuilt PYNQ sdist (no bitstream rebuilding)
3. ✅ Build Stage 3: Install PYNQ core packages on rootfs
4. ✅ Build Stage 4: Board-specific STAR-Z2 packages
5. ✅ Create boot files with PetaLinux
6. ✅ Package everything into SD card image

## Expected Output
After successful completion:
- `PYNQ/sdbuild/output/STAR-Z2-3.0.1.img` - Your SD card image!
- Ready to flash to SD card

## If You Get Errors

### Error: "PYNQ_ROOTFS file does not exist"
You forgot Step 1. Create the symlink.

### Error: Board spec or BSP issues
These are configuration issues we can fix.

### Error: PetaLinux build issues
May need to adjust board configuration.

## Monitor Progress
```bash
# In another terminal, watch the build
tail -f /tmp/build_log.txt  # if you redirect output

# Or just watch the terminal output
# Look for:
# - "Building stage 3"
# - "Building STAR-Z2"
# - "Creating image"
```

## Total Expected Time
- Stage 3: ~10-15 minutes
- Stage 4 + PetaLinux: ~20-30 minutes
- **Total: 30-45 minutes**

Much better than 6+ hours!
