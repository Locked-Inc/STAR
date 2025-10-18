# Git Cache Setup for PYNQ Builds

This document explains how to set up and use git reference caching for faster PYNQ image builds.

## Overview

Git caching uses git's built-in reference mechanism to avoid downloading the same objects multiple times. When repositories are cloned repeatedly during builds, the cache provides local copies of git objects, dramatically reducing clone times and bandwidth usage.

This works alongside apt-cacher-ng and pip cache to provide comprehensive caching for builds.

## Benefits

- **Faster git clones**: Clone operations use local references instead of downloading
- **Reduced bandwidth**: Git objects downloaded once and shared across builds
- **Automatic deduplication**: Git handles object sharing automatically
- **Safe**: Uses git's native reference mechanism (read-only)
- **Persistent**: Cache survives across builds and git operations

## Quick Start

### 1. Set Up the Cache

Run the setup script to create and optionally pre-populate the git cache:

```bash
./setup-git-cache.sh
```

This will:
- Create a cache directory at `~/.cache/pynq-build/git`
- Create a configuration file (`.git-cache-config`)
- Optionally pre-populate with common repositories
- Display cache information

### 2. Run Your Build

The cache is now ready to use:

```bash
./build.sh
```

The build script will automatically:
- Detect the git cache configuration
- Display cache status and size
- Pass the cache directory to git operations
- Use cached objects when cloning repositories

## How It Works

### Cache Location

By default, the git cache is stored at:
```
~/.cache/pynq-build/git/
```

This location is configurable in the `.git-cache-config` file.

### Reference Mechanism

Git caching uses the `--reference` flag:

```bash
git clone --reference ~/.cache/pynq-build/git/repo_mirror URL
```

This tells git to:
- Use cached objects from the reference repository
- Only download new or missing objects
- Create a working repository that shares objects with the cache
- Maintain full functionality (the cache is read-only)

### Cache Structure

Each cached repository is stored as a mirror (bare repository):

```
~/.cache/pynq-build/git/
├── github.com_Xilinx_PYNQ.git/
│   ├── objects/
│   ├── refs/
│   └── config
├── github.com_user_repo.git/
│   └── ...
└── ...
```

### Build Integration

The git cache is integrated through:

1. **build.sh** - Checks for cache configuration and exports `GIT_CACHE_DIR`
2. **Environment variable** - `GIT_CACHE_DIR` is available to all build scripts
3. **Git commands** - Modified to use `--reference` when cache is available

## Usage Patterns

### First Build
- Repositories are cloned normally
- Objects are cached in the reference repository
- Clone time: Normal

### Subsequent Builds
- Git uses cached objects as references
- Only new commits are downloaded
- Clone time: Significantly faster (can be 10x+ faster)

## Cache Management

### View Cache Size

```bash
du -sh ~/.cache/pynq-build/git
```

### List Cached Repositories

```bash
ls -lh ~/.cache/pynq-build/git
```

### Update Cached Repositories

Keep the cache up-to-date with remote repositories:

```bash
for repo in ~/.cache/pynq-build/git/*; do
    echo "Updating $(basename $repo)..."
    (cd "$repo" && git fetch --all --prune)
done
```

Or update a specific repository:

```bash
cd ~/.cache/pynq-build/git/github.com_Xilinx_PYNQ.git
git fetch --all --prune
```

### Clear the Cache

If you need to clear the cache and start fresh:

```bash
rm -rf ~/.cache/pynq-build/git
./setup-git-cache.sh
```

### Disable the Cache

To temporarily disable the cache, rename or remove the configuration file:

```bash
mv .git-cache-config .git-cache-config.disabled
```

The build will still work but won't use the git cache.

## Typical Cache Sizes

Expected cache sizes based on PYNQ build requirements:

- **Initial (empty)**: 0 MB
- **After pre-population**: 500 MB - 1 GB
- **After first build**: 1-2 GB
- **After multiple builds**: 2-3 GB

The cache size depends on the number and size of repositories cloned during builds.

## Comparison with Other Caches

All three caching systems work together:

| Feature | apt-cacher-ng | pip cache | git cache |
|---------|--------------|-----------|-----------|
| Purpose | System packages | Python packages | Git repositories |
| Required | Yes | Recommended | Recommended |
| Setup | `./setup-apt-cache.sh` | `./setup-pip-cache.sh` | `./setup-git-cache.sh` |
| Cache Size | ~1-2 GB | ~100-150 MB | ~2-3 GB |
| Time Saved | 10-15 minutes | 3-5 minutes | 2-5 minutes |

## Troubleshooting

### Cache Not Being Used

Check that:
1. `.git-cache-config` exists in the project root
2. The cache directory exists and is readable
3. The build script shows "Git Cache: ENABLED" in the configuration output
4. Git commands include `--reference` flag in build logs

### Missing Objects Error

If you see "missing objects" errors:

```bash
# Clear and rebuild the cache
rm -rf ~/.cache/pynq-build/git
./setup-git-cache.sh
```

This can happen if cached repositories become corrupted or out of sync.

### Permission Issues

The cache directory should be owned by your user (not root):

```bash
ls -ld ~/.cache/pynq-build/git
# Should show your username, not root
```

If owned by root, fix with:

```bash
sudo rm -rf ~/.cache/pynq-build/git
./setup-git-cache.sh
```

### Disk Space Issues

The git cache can grow to 2-3 GB. Check available space:

```bash
df -h ~/.cache
```

If needed, clear the cache:

```bash
rm -rf ~/.cache/pynq-build/git
```

## Advanced Configuration

### Custom Cache Location

Edit `.git-cache-config` to change the cache directory:

```bash
# .git-cache-config
GIT_CACHE_DIR=/path/to/your/custom/cache
```

Make sure the directory exists and is writable.

### Shared Cache (Multiple Users)

To share a cache between users:

1. Create a shared cache directory:
   ```bash
   sudo mkdir -p /opt/pynq-build-cache/git
   sudo chmod 1777 /opt/pynq-build-cache/git  # Sticky bit + world writable
   ```

2. Update each user's `.git-cache-config`:
   ```bash
   GIT_CACHE_DIR=/opt/pynq-build-cache/git
   ```

### Pre-populate Specific Repositories

To manually add repositories to the cache:

```bash
cd ~/.cache/pynq-build/git
git clone --mirror https://github.com/user/repo.git
```

The repository will be named based on its URL.

### Using Cache Outside Builds

You can use the cache for manual git operations:

```bash
# Clone using the cache
git clone --reference ~/.cache/pynq-build/git/github.com_Xilinx_PYNQ.git \
    https://github.com/Xilinx/PYNQ.git my-pynq

# Or with dissociate to make it standalone after clone
git clone --reference ~/.cache/pynq-build/git/github.com_Xilinx_PYNQ.git \
    --dissociate https://github.com/Xilinx/PYNQ.git my-pynq
```

## Technical Details

### How Git References Work

When you clone with `--reference`:
1. Git reads objects from the reference repository
2. Only new/missing objects are downloaded
3. The working repository stores a reference to the cache
4. Objects are shared (not copied) saving disk space
5. The cache is read-only from the working repository's perspective

### Safety

- The cache cannot be corrupted by working repositories
- Working repositories can be deleted without affecting the cache
- The cache can be deleted without affecting working repositories (but objects may need re-download)
- Git's object storage is content-addressable (safe for sharing)

### Environment Variables

- `GIT_CACHE_DIR`: Path to the git cache directory
- Set by `build.sh` from `.git-cache-config`
- Used by build scripts to add `--reference` flags

### Automatic Updates

The cache can be kept up-to-date by periodically fetching:

```bash
# Add to crontab for weekly updates
0 2 * * 0 for repo in ~/.cache/pynq-build/git/*; do (cd "$repo" && git fetch --all --prune); done
```

## Best Practices

1. **Run setup before first build**: Pre-populate the cache to maximize benefits
2. **Periodic updates**: Update cached repositories weekly for best performance
3. **Monitor size**: Check cache size occasionally and clear if needed
4. **Combine with other caches**: Use all three caching systems for maximum speed
5. **Shared environments**: Use a shared cache for teams working on the same machine

## Performance Impact

### Example: PYNQ Repository

Without cache:
```bash
time git clone https://github.com/Xilinx/PYNQ.git
# ~2-5 minutes depending on network speed
```

With cache (after initial population):
```bash
time git clone --reference ~/.cache/pynq-build/git/github.com_Xilinx_PYNQ.git \
    https://github.com/Xilinx/PYNQ.git
# ~10-30 seconds
```

### Build Time Savings

Depending on how many repositories are cloned during builds:
- First build: Minimal savings (cache is being populated)
- Subsequent builds: 2-5 minutes saved on git operations
- Rebuilds with updates: Even faster as only deltas are downloaded

## See Also

- [apt-cache-setup.md](apt-cache-setup.md) - System package caching
- [pip-cache-setup.md](pip-cache-setup.md) - Python package caching
- [Git reference documentation](https://git-scm.com/docs/git-clone#Documentation/git-clone.txt---reference-if-ableltrepositorygt)
