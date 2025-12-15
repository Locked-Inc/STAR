# Build All Examples Scripts

Scripts to compile all 305 examples and generate build reports.

## Quick Start

### Windows Users (RECOMMENDED)

**Option 1: PowerShell (Fastest - Parallel Builds)**

```powershell
# First time only - Allow script execution
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# Then run the script
.\build_all_examples_parallel.ps1
```

**Option 2: Batch File (Slower - Sequential)**

Just double-click `build_all_examples.bat` or run:
```cmd
build_all_examples.bat
```

### Linux/Mac/WSL Users

**Parallel (Faster):**
```bash
chmod +x build_all_examples_parallel.sh
./build_all_examples_parallel.sh 8
```

**Sequential:**
```bash
chmod +x build_all_examples.sh
./build_all_examples.sh
```

## Windows Script Execution Issues

If Windows blocks the PowerShell script, you have two options:

### Option 1: Enable Scripts (One Time)
Open PowerShell as Administrator and run:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```
Then run the script normally.

### Option 2: Bypass for Single Run
```powershell
PowerShell -ExecutionPolicy Bypass -File .\build_all_examples_parallel.ps1
```

### Option 3: Use the Batch File
The `.bat` file always works without permission issues:
```cmd
build_all_examples.bat
```

## What You Get

After running, you'll have a `build_logs/` folder with:
- **One log per example** (305 files) - Full build output
- **Overview summary** - Quick view of SUCCESS/FAILED examples

Example output:
```
build_logs/
├── build_overview_20250121_143022.txt  ← Check this first!
├── example_001_i2c_basic_20250121_143022.log
├── example_002_spi_basic_20250121_143022.log
└── ... (305 total)
```

## Build Time Estimates

| Script | Time (8-core CPU) |
|--------|-------------------|
| Parallel PowerShell/Bash | ~30-60 minutes |
| Sequential Batch/Bash | ~4-6 hours |

## Customizing Parallel Builds

Adjust based on your CPU cores:

**PowerShell:**
```powershell
.\build_all_examples_parallel.ps1 -MaxParallel 4
```

**Bash:**
```bash
./build_all_examples_parallel.sh 16
```

## Detailed Documentation

See `BUILD_SCRIPTS_README.md` for complete documentation.
