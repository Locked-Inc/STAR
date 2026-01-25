# STAR DevContainer Configuration

This devcontainer provides a consistent development environment for the STAR project.

## Prerequisites

Before rebuilding the container, authenticate CLI tools on your **host machine**:

```bash
# On your host (outside container)
gemini-cli auth login
claude-code auth login
```

This creates credential files in `~/.config/gemini/` and `~/.config/claude/` which are bind-mounted into the container.

## What's Installed

### Official Features

- Git LFS
- GitHub CLI
- Go
- Node.js
- Python

### Community Features

- Act (GitHub Actions runner)
- Claude Code CLI
- Gemini CLI
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
Host: ~/.config/gemini  →  Container: /home/vscode/.config/gemini
Host: ~/.config/claude  →  Container: /home/vscode/.config/claude
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
mkdir -p ~/.config/gemini ~/.config/claude
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
