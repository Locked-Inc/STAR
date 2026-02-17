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
npm i -g opencode-ai @openai/codex  # Install OpenCode AI and OpenAI Codex CLI
opencode /connect                    # Configure OpenCode AI providers
codex auth login                     # Authenticate OpenAI Codex CLI
```

This creates credential files in `~/.config/` which are bind-mounted into the container.

## What's Installed

### Base Image (osrf/ros:jazzy-desktop)

The base Docker image includes:
- ROS 2 Jazzy Desktop (full installation)
- Python 3 (via apt, not devcontainer feature)
- nanopb for Protocol Buffer C code generation
- Buf CLI for Protocol Buffer development
- Doxygen + minimal LaTeX for PDF documentation

### DevContainer Features (5)

- Git LFS
- GitHub CLI
- Go
- Node.js
- Docker-in-Docker

### VS Code Extensions (12)

- Go (golang.go)
- C/C++ with clangd LSP (llvm-vs-code-extensions.vscode-clangd)
- C/C++ debugger CodeLLDB (vadimcn.vscode-lldb)
- CMake Tools (ms-vscode.cmake-tools)
- Python (ms-python.python, ms-python.vscode-pylance)
- Protocol Buffers (bufbuild.vscode-buf)
- Git tools (eamodio.gitlens, mhutchie.git-graph)
- Linting/formatting (dbaeumer.vscode-eslint, esbenp.prettier-vscode)
- YAML (redhat.vscode-yaml)

**C++ Toolchain Architecture**:

- **clangd**: Provides IntelliSense/LSP (code completion, diagnostics)
- **CodeLLDB**: Provides debugging (F5, breakpoints, stepping)
- **CMake Tools**: Automatically generates `compile_commands.json` for clangd

This setup avoids the `ms-vscode.cpptools` extension which conflicts with clangd's IntelliSense. The `C_Cpp.intelliSenseEngine` setting is explicitly disabled to prevent conflicts.

## Credential Persistence

CLI tool credentials persist across container rebuilds via explicit bind mounts:

```
Host: ~/.config/gh/       →  Container: /home/star/.config/gh/
Host: ~/.config/opencode/ →  Container: /home/star/.config/opencode/
Host: ~/.config/openai/   →  Container: /home/star/.config/openai/
Host: ~/.ssh/             →  Container: /home/star/.ssh/
Host: ~/.gitconfig        →  Container: /home/star/.gitconfig
```

Only the specific subdirectories listed above are mounted. This prevents host-specific files (iTerm2 sockets, dconf databases, PulseAudio configs) from leaking into the container and works identically on macOS, Windows, and Linux.

**Windows users**: The `initializeCommand` (`init.cmd`) creates these directories automatically.

## Lifecycle Commands

The devcontainer uses split lifecycle commands for optimal Codespaces prebuild support:

| Command | When | What |
|---------|------|------|
| `initializeCommand` | Before container creation | Creates host directories for bind mounts |
| `onCreateCommand` | After container creation (cached in prebuilds) | Creates Codespaces fallback dirs, installs golangci-lint, npm tools |
| `updateContentCommand` | After content update | Runs `buf mod update` |

## Known Limitations

### Codespaces Bind Mounts

GitHub Codespaces has no host machine, so all bind mounts are silently ignored. The `onCreateCommand` creates fallback directories (`~/.config`, `~/.ssh`, `~/.gitconfig`) to ensure tools work, but credentials won't persist across Codespace rebuilds. Use Codespaces Secrets for sensitive values.

### Cross-Platform Mounts

The mount syntax relies on VS Code resolving undefined variables to empty strings:

- Linux/Mac: `HOME` is defined, `USERPROFILE` resolves to `""`
- Windows: `USERPROFILE` is defined, `HOME` resolves to `""`

If both are defined (e.g., Cygwin, Git Bash), the path becomes invalid. In that case, edit `devcontainer.json` to use only one variable.

### LaTeX Package Availability

The Dockerfile installs a minimal LaTeX distribution for Doxygen PDF output. If `latexmk -pdf star_documentation.tex` fails with missing package errors, add the required `texlive-*` package. Likely candidates:

- `texlive-latex-extra` — tikz extras, booktabs, enumitem, etc.
- `texlive-science` — algorithm packages

### Codespaces Prebuilds

For instant Codespaces startup, enable prebuilds in your repository settings (Settings → Codespaces) or add a `.github/workflows/codespaces-prebuild.yml`. Without prebuilds, first Codespaces startup still requires `onCreateCommand` to run.

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

**Fix**: Rebuild the container to apply the fix:

```
Command Palette (Ctrl+Shift+P)
→ Dev Containers: Rebuild Container Without Cache
```

### "cppbuild" task type error

**Error**: `there is no registered task type 'cppbuild'`

**Explanation**: This is a harmless VS Code glitch. Our project uses `shell` tasks, not `cppbuild` tasks.

**Fix**: Ignore this error or reload the window:

```
Ctrl+Shift+P → "Developer: Reload Window"
```

### "clangd not found on PATH"

**Error**: `The 'clangd' language server was not found on your PATH`

**Fix**: The Dockerfile installs `clangd` via apt. Rebuild the container:

```
Ctrl+Shift+P → "Dev Containers: Rebuild Container"
```

### "No such file or directory" on mount

The credential directories don't exist on your host. The `initializeCommand` should create them automatically, but you can also create them manually:

```bash
mkdir -p ~/.ssh ~/.config/gh ~/.config/opencode ~/.config/openai
touch ~/.gitconfig
```

### C++ IntelliSense conflicts

If you added `ms-vscode.cpptools` alongside `clangd`, both will try to provide IntelliSense. This configuration uses:

- **clangd** for IntelliSense (faster, more accurate)
- **CodeLLDB** for debugging (lighter than cpptools)

The `C_Cpp.intelliSenseEngine` setting is disabled by default. If you need `cpptools` for specific debugging scenarios, uninstall clangd or use the extension's disable command.

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
- **Performance**: BuildKit cache mounts, minimal LaTeX, reduced features

We do **not** use extension volumes or persistent state that would break reproducibility.
