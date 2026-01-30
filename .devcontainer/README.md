# STAR DevContainer Configuration

This devcontainer provides a consistent development environment for the STAR project.

## Prerequisites

### Docker Desktop Configuration (Apple Silicon)

If you're on an Apple Silicon Mac (M1/M2/M3), the devcontainer uses AMD64 emulation via Rosetta 2 because ROS Jazzy Desktop doesn't have ARM64 images. Ensure Rosetta is enabled in Docker Desktop:

1. Open Docker Desktop Settings
2. Go to "Features in development"
3. Enable "Use Rosetta for x86_64/amd64 emulation on Apple Silicon"

### CLI Authentication

Before rebuilding the container, authenticate CLI tools on your **host machine**:

```bash
# On your host (outside container)
gemini-cli auth login
claude-code auth login
npm i -g opencode-ai @openai/codex  # Install OpenCode AI and OpenAI Codex CLI
opencode /connect                    # Configure OpenCode AI providers
codex auth login                     # Authenticate OpenAI Codex CLI
```

This creates credential files in `~/.config/gemini/`, `~/.config/claude/`, `~/.config/opencode/`, and `~/.config/openai/` which are bind-mounted into the container.

## What's Installed

### Base Image (osrf/ros:jazzy-desktop)

The base Docker image includes:
- ROS 2 Jazzy Desktop (full installation)
- Python 3 (via apt, not devcontainer feature)
- nanopb for Protocol Buffer C code generation
- Buf CLI for Protocol Buffer development

### Official Features

- Git LFS
- GitHub CLI
- Go
- Node.js

### Community Features

- Act (GitHub Actions runner)
- Claude Code CLI
- Gemini CLI
- OpenCode AI CLI (npm package)
- OpenAI Codex CLI (npm package)
- Actionlint

### VS Code Extensions

All extensions listed in `.vscode/extensions.json` are auto-installed, including:

- Go (golang.go)
- C/C++ with clangd LSP (llvm-vs-code-extensions.vscode-clangd)
- C/C++ debugger CodeLLDB (vadimcn.vscode-lldb)
- CMake Tools (ms-vscode.cmake-tools)
- ROS2 (ms-iot.vscode-ros)
- Python (ms-python.python, ms-python.vscode-pylance)
- Protocol Buffers (bufbuild.vscode-buf)
- And 15+ more utilities

**C++ Toolchain Architecture**:

- **clangd**: Provides IntelliSense/LSP (code completion, diagnostics)
- **CodeLLDB**: Provides debugging (F5, breakpoints, stepping)
- **CMake Tools**: Automatically generates `compile_commands.json` for clangd

This setup avoids the `ms-vscode.cpptools` extension which conflicts with clangd's IntelliSense.

## Credential Persistence

CLI tool credentials persist across container rebuilds via bind mounts:

```
Host: ~/.config/gemini   →  Container: /home/star/.config/gemini
Host: ~/.config/claude   →  Container: /home/star/.config/claude
Host: ~/.config/opencode →  Container: /home/star/.config/opencode
Host: ~/.config/openai   →  Container: /home/star/.config/openai
Host: ~/.config/gh       →  Container: /home/star/.config/gh
```

**Windows users**: Ensure these directories exist before rebuilding. The mount syntax `${localEnv:HOME}${localEnv:USERPROFILE}` works on most systems but may fail on mixed environments (Git Bash with both env vars set).

## Known Limitations

### Cross-Platform Mounts

The mount syntax relies on VS Code resolving undefined variables to empty strings:

- Linux/Mac: `HOME` is defined, `USERPROFILE` resolves to `""`
- Windows: `USERPROFILE` is defined, `HOME` resolves to `""`

If both are defined (e.g., Cygwin, Git Bash), the path becomes invalid. In that case, edit `devcontainer.json` to use only one variable.

### Extension Installation

Extensions are installed from the VS Code marketplace during container creation. First build may take 5-10 minutes. Subsequent rebuilds are faster due to Docker layer caching.

If extensions fail to install, check:

1. Network connectivity to marketplace
2. Disk space (~500MB needed)
3. Output logs: Command Palette → `Dev Containers: Show All Logs`

## Troubleshooting

### Platform mismatch warning (Apple Silicon)

**Error**: `Base image osrf/ros:jazzy-desktop was pulled with platform "linux/amd64", expected "linux/arm64"`

**Explanation**: ROS Jazzy Desktop doesn't provide ARM64 images. The devcontainer is configured to use AMD64 emulation via Rosetta 2.

**Fix**: Enable Rosetta in Docker Desktop (see Prerequisites section above), then rebuild:

```
Command Palette (Ctrl+Shift+P)
→ Dev Containers: Rebuild Container Without Cache
```

### Yarn GPG key error during build

**Error**: `NO_PUBKEY 62D54FD4003F6525` / `The repository 'https://dl.yarnpkg.com/debian stable InRelease' is not signed`

**Explanation**: The Yarn repository was present in the base image but lacked proper GPG keys. Since we only need npm (not Yarn), the Yarn repository is now removed before package updates.

**Fix**: The `postCreateCommand` in `devcontainer.json` now removes the Yarn repository list file before running `apt-get update`. Rebuild the container to apply the fix:

```
Command Palette (Ctrl+Shift+P)
→ Dev Containers: Rebuild Container Without Cache
```

### "cppbuild" task type error

**Error**: `there is no registered task type 'cppbuild'`

**Explanation**: This is a harmless VS Code glitch. The error appears because:

1. VS Code scans all tasks in `tasks.json` during startup
2. Some cache or indexing may reference a non-existent `cppbuild` task
3. Our project uses `shell` tasks, not `cppbuild` tasks

**Fix**: Ignore this error or reload the window:

```
Ctrl+Shift+P → "Developer: Reload Window"
```

**Verification**: Run `grep -r "cppbuild" .vscode/` - you'll find no such tasks defined.

### "clangd not found on PATH"

**Error**: `The 'clangd' language server was not found on your PATH`

**Fix**: The Dockerfile now installs `clangd` via apt. Rebuild the container:

```
Ctrl+Shift+P → "Dev Containers: Rebuild Container"
```

If clangd is still not found after rebuild, check version:

```bash
which clangd
clangd --version
```

Ubuntu/Debian's clangd package provides an older but stable version. For the latest clangd (21.x), the extension can download it, but this won't persist across rebuilds.

### "No such file or directory" on mount

The credential directories don't exist on your host. Create them:

```bash
mkdir -p ~/.config/gemini ~/.config/claude ~/.config/opencode ~/.config/openai ~/.config/gh
```

### Extensions not activating

After rebuild, reload the window:

```
Ctrl+Shift+P → "Developer: Reload Window"
```

### C++ IntelliSense conflicts

If you added `ms-vscode.cpptools` alongside `clangd`, both will try to provide IntelliSense. This configuration uses:

- **clangd** for IntelliSense (faster, more accurate)
- **CodeLLDB** for debugging (lighter than cpptools)

If you need `cpptools` for specific debugging scenarios, add to settings:

```json
"C_Cpp.intelliSenseEngine": "disabled"
```

### clangd not finding headers

Ensure CMake has generated `compile_commands.json`:

```bash
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

The devcontainer already configures CMake to do this automatically via `cmake.configureArgs`.

## Rebuild

To apply configuration changes:

```
Command Palette (Ctrl+Shift+P)
→ Dev Containers: Rebuild Container
```

For a clean rebuild (clears Docker cache):

```
→ Dev Containers: Rebuild Container Without Cache
```

## Architecture

This configuration prioritizes:

- **Declarative setup**: All tools/extensions defined in code
- **Layer caching**: Docker caches extension installations between builds
- **Team consistency**: Everyone gets the same toolchain

We do **not** use extension volumes or persistent state that would break reproducibility.
