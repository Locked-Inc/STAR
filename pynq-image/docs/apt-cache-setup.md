# apt-cacher-ng Setup for Faster PYNQ Builds

## Overview

When building PYNQ images repeatedly for testing, the build process downloads hundreds of packages from Ubuntu repositories each time. This can take 15-30 minutes per build depending on your internet connection.

**REQUIRED**: The build script now requires `apt-cacher-ng` to be set up and running. This caches packages locally after the first build, reducing subsequent build times significantly (potentially saving 10-20 minutes per build).

You must run the setup script before your first build.

## Quick Setup

1. Run the setup script:
   ```bash
   cd pynq-image
   ./setup-apt-cache.sh
   ```

2. The script will:
   - Install apt-cacher-ng
   - Configure it for optimal PYNQ builds
   - Enable the service to start on boot
   - Optionally configure your host system to use the cache

3. Run your build as normal:
   ```bash
   ./build.sh
   ```

   The build script will verify that apt-cacher-ng is running before starting.

## How It Works

### First Build
- Packages are downloaded from Ubuntu repositories
- apt-cacher-ng caches them locally in `/var/cache/apt-cacher-ng`
- Build time: ~30-45 minutes (normal)

### Subsequent Builds
- Packages are served from local cache (no internet download)
- Only new or updated packages are downloaded
- Build time: ~15-25 minutes (much faster!)

## Monitoring Cache

### Web Interface

**Main page:**
http://localhost:3142

From there, click "Statistics report and configuration page" to view:
- Cache hit/miss statistics
- Storage usage and disk space
- Cached package counts
- Recent activity logs
- Configuration details

**Direct statistics link:**
http://localhost:3142/acng-report.html

**Note:** The web interface is enabled by default with no authentication. If you want to add password protection, edit `/etc/apt-cacher-ng/acng.conf` and uncomment the `AdminAuth` line.

### Check Cache Size
```bash
du -sh /var/cache/apt-cacher-ng
```

### View Service Status
```bash
sudo systemctl status apt-cacher-ng
```

### View Logs
```bash
# Follow live logs
sudo journalctl -u apt-cacher-ng -f

# View recent activity
sudo tail -f /var/log/apt-cacher-ng/apt-cacher.log

# See cache hits vs misses during build
sudo journalctl -u apt-cacher-ng -f | grep -E "MISS|HIT"
```

### Monitor During Build

Open a second terminal while building and run:
```bash
# Watch cache size grow in real-time
watch -n 2 'du -sh /var/cache/apt-cacher-ng'

# Or count .deb files being cached
watch -n 5 'find /var/cache/apt-cacher-ng -name "*.deb" | wc -l'
```

**First build:** You'll see lots of MISS entries (downloading and caching).
**Subsequent builds:** You'll see HIT entries (serving from cache).

## Managing the Cache

### Clear Cache
If you need to free up disk space or force fresh downloads:
```bash
sudo rm -rf /var/cache/apt-cacher-ng/*
sudo systemctl restart apt-cacher-ng
```

### Disable Cache Temporarily
**WARNING**: The build script will fail if apt-cacher-ng is not running.

If you need to stop the cache service:
```bash
sudo systemctl stop apt-cacher-ng
```

**Note**: You must restart it before building:
```bash
sudo systemctl start apt-cacher-ng
```

### Uninstall
To completely remove apt-cacher-ng:
```bash
sudo systemctl stop apt-cacher-ng
sudo systemctl disable apt-cacher-ng
sudo apt-get remove --purge apt-cacher-ng
sudo rm -rf /var/cache/apt-cacher-ng
```

If you configured your host system to use the cache, also remove:
```bash
sudo rm /etc/apt/apt.conf.d/01proxy
```

## Configuration Details

### Cache Location
- Default: `/var/cache/apt-cacher-ng`
- Can be changed in `/etc/apt-cacher-ng/acng.conf`

### Port
- Default: 3142
- Accessible at: http://localhost:3142

### Retention
- Packages are kept for 30 days after last access
- Adjust `ExTreshold` in `/etc/apt-cacher-ng/acng.conf` to change this

### What Gets Cached
- All .deb packages from Ubuntu repositories
- Package indices and metadata
- Both ARM (armhf) and ARM64 (aarch64) packages

### What Doesn't Get Cached
- Security updates are checked more frequently
- Release files are refreshed regularly
- HTTPS connections may bypass cache (HTTP is used for packages)

## Troubleshooting

### Build Fails: "apt-cacher-ng is not running"
The build script requires apt-cacher-ng to be running. Check the service status:
```bash
sudo systemctl status apt-cacher-ng
```

If it's not running, start it:
```bash
sudo systemctl start apt-cacher-ng
```

If you haven't set it up yet, run:
```bash
./setup-apt-cache.sh
```

### Build Fails with Cache Running
If the cache is running but builds still fail, try clearing the cache:
```bash
sudo rm -rf /var/cache/apt-cacher-ng/*
sudo systemctl start apt-cacher-ng
```

### Disk Space Issues
The cache can grow to 5-10GB depending on what you build. Check available space:
```bash
df -h /var/cache
```

If needed, clear old cached packages:
```bash
sudo rm -rf /var/cache/apt-cacher-ng/*
```

### Permission Issues
If you see permission errors, ensure apt-cacher-ng has correct ownership:
```bash
sudo chown -R apt-cacher-ng:apt-cacher-ng /var/cache/apt-cacher-ng
```

## Performance Tips

1. **First Build**: Expect normal build times as the cache is populated
2. **Incremental Changes**: Cache is most effective when rebuilding with minor changes
3. **Network Speed**: On slow connections, the cache provides the most benefit
4. **Disk Space**: Ensure you have at least 10GB free in `/var/cache`
5. **Host Configuration**: Configure your host system to use the cache for faster apt operations

## Architecture Support

The cache automatically handles multiple architectures:
- **armhf** (32-bit ARM) - for Zynq-7000 boards
- **aarch64** (64-bit ARM) - for Zynq UltraScale+ boards
- **amd64** (x86_64) - if you configure your host to use the cache

All architectures share the same cache, maximizing reuse.

## Technical Details

### How Proxy Works
- Build script sets `http_proxy` environment variable
- multistrap honors this proxy setting
- apt within the chroot also uses the proxy
- Packages are downloaded through apt-cacher-ng on first access
- Subsequent requests are served from local cache

### Configuration Files Modified
- `/etc/apt-cacher-ng/acng.conf` - Main configuration
- PYNQ multistrap configs include proxy comments (no functional changes)
- `build.sh` - Detects and enables cache automatically

### No PYNQ Code Changes Required
The cache integration is transparent to PYNQ's build system. The standard `http_proxy` environment variable is used, which multistrap and apt already support natively.
