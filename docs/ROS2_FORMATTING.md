# ROS2 Code Formatting Guide

## The Problem

ROS2 uses **ament_uncrustify** as the official code formatter, but many editors (including VS Code's C++ extension) default to **clang-format**. These formatters have different style preferences, leading to CI failures even when code looks formatted locally.

**Common divergences:**
- Reference/pointer spacing: `Type & ref` (uncrustify) vs `Type& ref` (clang-format)
- Function parameter alignment
- Initializer list formatting
- Header guard patterns

## The Solution

This project now uses `ament_uncrustify` exclusively for ROS2 C++ code formatting to match CI expectations.

## Usage

### Option 1: Inside Devcontainer (Recommended)

```bash
# Format all ROS2 packages
./scripts/ros2/format-ros2.sh

# Check without modifying (CI mode)
./scripts/ros2/format-ros2.sh --check

# Verbose output
./scripts/ros2/format-ros2.sh -v

# Skip header guard checking
./scripts/ros2/format-ros2.sh --skip-guards
```

### Option 2: From Host Machine (with Docker)

```bash
# Uses Docker to run formatting in the devcontainer
./scripts/ros2/format-ros2-docker.sh

# All the same options work
./scripts/ros2/format-ros2-docker.sh --check
```

### Manual Formatting

For a specific package:
```bash
cd star-ros2/src/<package_name>
ament_uncrustify --reformat
```

## Pre-Commit Hook

The pre-commit hook **script** lives at `scripts/git/pre-commit`. It is not
installed by default; opt in with one of:

```bash
# Recommended: point Git at the tracked hooks directory (project-wide)
git config core.hooksPath scripts/git

# Or, symlink just the pre-commit hook
ln -s ../../scripts/git/pre-commit .git/hooks/pre-commit
```

When installed, it automatically checks formatting before allowing commits.
It will:

1. Only run if ROS2 `.cpp`/`.hpp` files are staged.
2. Check both code formatting and (project-wide) ASCII / `#pragma once`
   policy.
3. Block the commit if issues are found and provide instructions to fix.

To bypass (not recommended):
```bash
git commit --no-verify
```

## VS Code Settings

The workspace settings in `.vscode/settings.json` now:

1. **Disable auto-formatting** for C++ files to prevent clang-format conflicts.
2. Set formatOnSave to false for C++ (manual formatting with
   `./scripts/ros2/format-ros2.sh`).

## Header Guards

CLAUDE.md mandates **`#pragma once`** for ALL C/C++ headers in this
project, including ROS2 packages. Use it consistently:

```cpp
// File: star-ros2/src/star_spi_bridge/include/star_spi_bridge/spi_driver.hpp

#pragma once

// ... code ...
```

`ament_uncrustify` does not require traditional include guards; the
project policy is to use `#pragma once` everywhere. The
`scripts/ros2/fix-header-guards.sh` helper (referenced under "Files
Used" below) rewrites legacy `#ifndef`/`#define`/`#endif` triplets
to `#pragma once`.

## CI Integration

The format check is integrated into the ROS2 build/test workflow:

```bash
# What CI runs
colcon test --packages-select <package>
# This includes ament_uncrustify and ament_cpplint checks
```

## Troubleshooting

### "ament_uncrustify not found"

You need to run the script in an environment with ROS2:
- Inside the devcontainer (recommended)
- Use `./scripts/ros2/format-ros2-docker.sh` from host
- Install ROS2 locally and source it

### VS Code still auto-formatting with clang-format

Check that `.vscode/settings.json` has:
```json
{
    "C_Cpp.formatting": "disabled",
    "[cpp]": {
        "editor.formatOnSave": false
    }
}
```

### Format script passes but CI still fails

Make sure you ran the script **inside the devcontainer** or via Docker wrapper, not on the host with a different version of uncrustify.

## Best Practices

1. **Always format before committing:**
   ```bash
    ./scripts/ros2/format-ros2.sh
   git add .
   git commit
   ```

2. **Let the pre-commit hook protect you** - don't bypass it

3. **Don't manually format** ROS2 C++ files in VS Code

4. **Use the script** even for small changes to ensure consistency

## Files Used

- `scripts/ros2/format-ros2.sh` -- main formatting script (uses uncrustify)
- `scripts/ros2/format-ros2-docker.sh` -- Docker wrapper for host usage
- `scripts/ros2/fix-header-guards.sh` -- legacy-guard -> `#pragma once` rewriter
- `scripts/git/pre-commit` -- pre-commit hook script (opt-in install via
  `git config core.hooksPath scripts/git`)
- `.vscode/settings.json` -- disables clang-format for C++

`star-ros2/.clang-format` is intentionally absent: ROS2 ament tooling
formats with `ament_uncrustify`, not clang-format.
