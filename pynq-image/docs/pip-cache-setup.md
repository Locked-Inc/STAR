# Pip Cache Setup for PYNQ Builds

This document explains how to set up and use pip package caching for faster PYNQ image builds.

## Overview

The pip cache system stores downloaded Python packages locally, so they don't need to be re-downloaded on every build. This works alongside the apt-cacher-ng system to provide comprehensive caching for both system packages and Python packages.

## Benefits

- **Faster builds**: Packages are downloaded once and reused across builds
- **Reduced bandwidth**: No need to re-download the same packages repeatedly
- **Offline capability**: Once cached, packages can be installed without internet
- **Complements apt-cacher-ng**: Together they cache both system and Python packages

## Quick Start

### 1. Set Up the Cache

Run the setup script to create and configure the pip cache directory:

```bash
./setup-pip-cache.sh
```

This will:
- Create a cache directory at `~/.cache/pynq-build/pip`
- Copy any existing pre-built wheels to the cache
- Create a configuration file (`.pip-cache-config`)
- Display cache information

### 2. Run Your Build

The cache is now ready to use:

```bash
./build.sh
```

The build script will automatically:
- Detect the pip cache configuration
- Display cache status and size
- Pass the cache directory to the build system
- Use cached packages when available

## How It Works

### Cache Location

By default, the pip cache is stored at:
```
~/.cache/pynq-build/pip/
```

This location is configurable in the `.pip-cache-config` file.

### Build Integration

The pip cache is integrated at multiple levels:

1. **build.sh** - Checks for cache configuration and exports `PIP_CACHE_DIR`
2. **python_packages_jammy/qemu.sh** - Uses cache for requirements.txt packages
3. **pynq/qemu.sh** - Uses cache for PYNQ-related packages

### Cache Behavior

- **First build**: Downloads packages and stores them in the cache
- **Subsequent builds**: Reuses cached packages (significantly faster)
- **Updates**: pip automatically checks for newer versions and updates cache as needed

## Cache Management

### View Cache Size

```bash
du -sh ~/.cache/pynq-build/pip
```

### Clear the Cache

If you need to clear the cache and start fresh:

```bash
rm -rf ~/.cache/pynq-build/pip
./setup-pip-cache.sh
```

### Disable the Cache

To temporarily disable the cache, rename or remove the configuration file:

```bash
mv .pip-cache-config .pip-cache-config.disabled
```

The build will still work but won't use the cache.

## Typical Cache Sizes

Expected cache sizes based on PYNQ build requirements:

- **Initial (with pre-built wheels)**: ~5-10 MB
- **After first build**: ~50-100 MB
- **After multiple builds**: ~100-150 MB

The cache size is modest compared to the build time savings.

## Comparison with apt-cacher-ng

Both caching systems work together:

| Feature | apt-cacher-ng | pip cache |
|---------|--------------|-----------|
| Purpose | System packages (deb) | Python packages (pip) |
| Required | Yes | Recommended |
| Setup | `./setup-apt-cache.sh` | `./setup-pip-cache.sh` |
| Cache Size | ~1-2 GB | ~100-150 MB |
| Time Saved | 10-15 minutes | 3-5 minutes |

## Troubleshooting

### Cache Not Being Used

Check that:
1. `.pip-cache-config` exists in the project root
2. The cache directory exists and is readable
3. The build script shows "Pip Cache: ENABLED" in the configuration output

### Cache Corruption

If you suspect cache corruption:

```bash
rm -rf ~/.cache/pynq-build/pip
./setup-pip-cache.sh
```

### Permission Issues

The cache directory should be owned by your user (not root):

```bash
ls -ld ~/.cache/pynq-build/pip
# Should show your username, not root
```

If owned by root, fix with:

```bash
sudo rm -rf ~/.cache/pynq-build/pip
./setup-pip-cache.sh
```

## Advanced Configuration

### Custom Cache Location

Edit `.pip-cache-config` to change the cache directory:

```bash
# .pip-cache-config
PIP_CACHE_DIR=/path/to/your/custom/cache
```

Make sure the directory exists and is writable.

### Shared Cache (Multiple Users)

To share a cache between users:

1. Create a shared cache directory:
   ```bash
   sudo mkdir -p /opt/pynq-build-cache/pip
   sudo chmod 1777 /opt/pynq-build-cache/pip  # Sticky bit + world writable
   ```

2. Update each user's `.pip-cache-config`:
   ```bash
   PIP_CACHE_DIR=/opt/pynq-build-cache/pip
   ```

## Pre-built Wheels

The PYNQ build system includes some pre-built wheels in:
```
PYNQ/sdbuild/packages/python_packages_jammy/pre-built/root/.cache/pip/wheels/
```

These are automatically copied to your cache when you run `./setup-pip-cache.sh`.

The build system has both:
- **Local wheels** (in the repo): Used as fallback
- **External cache** (this system): Persistent across builds and git operations

## Technical Details

### Environment Variables

- `PIP_CACHE_DIR`: Path to the pip cache directory
- Set by `build.sh` from `.pip-cache-config`
- Passed through to QEMU build environment

### pip Cache Options

The build uses pip's `--cache-dir` option:

```bash
python3 -m pip install --cache-dir $PIP_CACHE_DIR package-name
```

This tells pip to:
- Use cached wheels if available
- Store new downloads in the cache
- Automatically manage cache integrity

## See Also

- [apt-cache-setup.md](apt-cache-setup.md) - System package caching
- [build-process.md](build-process.md) - Overall build process documentation
- [pip documentation on caching](https://pip.pypa.io/en/stable/topics/caching/)
