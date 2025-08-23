# TDA4VM Yocto Build Setup

This directory contains the complete Yocto build environment for the TDA4VM J721EXSKG01EVM development board with **Docker-based development workflow**.

## 🚀 Quick Start

1. **Install Docker** (if not already installed):
   - **macOS**: [Docker Desktop](https://docs.docker.com/desktop/mac/install/)
   - **Linux**: [Docker Engine](https://docs.docker.com/engine/install/)
   - **Windows**: [Docker Desktop](https://docs.docker.com/desktop/windows/install/)

2. **One-command setup**:
   ```bash
   ./dev-setup.sh
   ```

3. **Start building**:
   ```bash
   make build        # Build EdgeAI image
   make shell        # Enter development environment
   ```

## 📋 System Information

- **Target Board**: TDA4VM J721EXSKG01EVM (SK-TDA4VM)
- **Configuration**: TI Processor SDK Analytics 11.01.07.05 (latest)
- **Yocto Version**: Scarthgap
- **Edge AI Support**: Included via meta-edgeai layer
- **Build Environment**: Docker-based for cross-platform compatibility

## 🏗️ Hardware Capabilities

- **AI Performance**: 8 TOPS deep learning performance  
- **Processors**: Dual Arm Cortex-A72, C7x DSP, GPU
- **Memory**: 4GB LPDDR4, 32GB eMMC
- **Applications**: Edge AI, smart cameras, robotics, machine vision

## 🛠️ Development Commands

### Easy Mode (Makefile)
```bash
make help           # Show all available commands
make setup          # Set up Docker environment  
make build          # Build EdgeAI image (recommended)
make build-minimal  # Build minimal test image
make shell          # Enter development shell
make clean          # Clean build artifacts
make status         # Show build information
```

### Advanced Mode (Docker Compose)
```bash
docker-compose up -d                    # Start container
docker-compose exec yocto-builder bash  # Enter shell
docker-compose down                     # Stop container
```

### Manual Build Commands (inside container)
```bash
cd build
. conf/setenv
MACHINE=j721e-evm bitbake tisdk-edgeai-image  # EdgeAI image
MACHINE=j721e-evm bitbake tisdk-default-image # Standard image  
MACHINE=j721e-evm bitbake core-image-minimal  # Minimal image
```

## 🎯 Available Images

- **`tisdk-edgeai-image`** - EdgeAI optimized (recommended for TDA4VM)
- **`tisdk-default-image`** - Standard SDK image
- **`core-image-minimal`** - Minimal testing image

## 🔧 Key Features Included

- **AI Frameworks**: TensorFlow Lite, ONNX Runtime, TVM
- **Vision Libraries**: OpenCV, GStreamer, hardware acceleration
- **Development Tools**: Docker, ROS, Python3, debugging tools
- **Connectivity**: WiFi, Ethernet, USB, CAN
- **Hardware Support**: GPU, DSP, AI accelerators

## 📁 Project Structure

```
yocto/
├── Dockerfile              # Container build definition
├── docker-compose.yml      # Development environment
├── Makefile                # Easy build commands
├── dev-setup.sh           # Automated setup script
├── build/                  # Yocto build directory
├── sources/                # Meta layers (TI, EdgeAI, etc.)
├── downloads/              # Source downloads (persistent)
├── sstate-cache/          # Build cache (persistent)
└── README_TDA4VM.md       # This file
```

## 💾 Output Location & Flashing

Built images are located in:
```
build/arago-tmp-external-arm-glibc/deploy/images/j721e-evm/
```

**Flash to SD Card**:
```bash
# Linux/macOS
sudo dd if=tisdk-edgeai-image-j721e-evm.wic of=/dev/sdX bs=1M status=progress

# Or use balenaEtcher (GUI)
```

## 🐛 Troubleshooting

### Container Issues
```bash
make reset-docker    # Reset everything
docker system prune  # Clean Docker cache
```

### Build Issues
```bash
make clean           # Clean build artifacts
make logs            # Show build logs
make status          # Check environment
```

### Missing Dependencies
The Docker container includes all required tools. On host:
- Ensure Docker is running
- Check available disk space (builds need 50GB+)
- Verify internet connection for downloads

## 🔄 Development Workflow

1. **Edit Code**: Use your favorite editor on host machine
2. **Files Auto-Sync**: Changes appear instantly in container
3. **Build**: Run `make build` or enter `make shell`
4. **Test**: Flash image to SD card and boot on TDA4VM
5. **Iterate**: Repeat cycle

## 📊 Build Performance

- **Initial Build**: 2-4 hours (downloads everything)
- **Incremental Builds**: 10-30 minutes
- **Cache Enabled**: Persistent downloads and sstate cache
- **Parallel Builds**: Auto-configured based on system CPU

## 🎓 Learning Resources

- [TI TDA4VM Documentation](https://www.ti.com/product/TDA4VM)
- [Yocto Project Manual](https://docs.yoctoproject.org/)
- [TI Processor SDK](https://software-dl.ti.com/jacinto7/esd/)
- [Edge AI Examples](https://github.com/TexasInstruments/edgeai-gst-apps)

---

**Ready to build embedded Linux for TDA4VM! 🚀**