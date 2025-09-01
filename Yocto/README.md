# Yocto Build Environment Setup

This document describes how to set up a QEMU-based virtual machine to build the Yocto image for the TDA4VM Dev Board.

## 1. Download the Ubuntu Server Image

Download the Ubuntu 22.04 LTS server image from the following URL and save it in this `Yocto` directory:

https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-amd64.img

## 2. Start the Virtual Machine

Run the following command from the Yocto directory to start the QEMU virtual machine:

```bash
qemu-system-x86_64 \
  -m 8192 \
  -smp 4 \
  -cpu qemu64 \
  -machine type=q35 \
  -drive file=jammy-server-cloudimg-amd64.img,if=virtio \
  -drive file=cloud-init.iso,media=cdrom \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -nographic
```

## 3. Connect to the Virtual Machine

Once the VM is running, you can connect to it using SSH. The username is `yoctouser` and the password is `yocto`.

```bash
ssh yoctouser@localhost -p 2222
```

## 4. Setup Yocto Build Environment for TI J721E (TDA4VM)

After connecting to the VM, run the automated setup script:

```bash
# From your host machine (in the Yocto directory):
./deploy-to-vm.sh
```

Or manually transfer and run the setup:

```bash
# Transfer setup script to VM
scp -P 2222 setup-yocto-j721e.sh yoctouser@localhost:/home/yoctouser/

# SSH into VM and run setup
ssh yoctouser@localhost -p 2222
chmod +x setup-yocto-j721e.sh
./setup-yocto-j721e.sh
```

## 5. Build Yocto Image for J721E TDA4VM

After setup is complete, build the image:

```bash
# SSH into the VM
ssh yoctouser@localhost -p 2222

# Navigate to build directory
cd yocto-build/build
source conf/setenv

# Build ADAS image for J721E (TDA4VM)
MACHINE=j721e-evm bitbake tisdk-adas-image
```

### Available Image Targets:
- `tisdk-adas-image`: Full ADAS filesystem with AI/ML capabilities
- `tisdk-edgeai-image`: EdgeAI optimized filesystem
- `tisdk-default-image`: Default filesystem
- `tisdk-base-image`: Minimal filesystem

### Build Output:
The built images will be located in:
```
yocto-build/build/arago-tmp-external-arm-glibc/deploy/images/j721e-evm/
```

## 6. Adding Custom Packages and Scripts

To add your custom packages and scripts to the image:

1. Create a custom meta-layer:
```bash
# In the VM, from yocto-build directory
mkdir -p meta-custom/recipes-custom/custom-scripts
```

2. Add your recipes and files to the custom layer
3. Add the layer to `bblayers.conf`
4. Rebuild the image

## Notes:
- The J721E machine configuration supports the TDA4VM SoC and J721EXSKG01EVM board
- Build time can be 2-4 hours depending on image complexity
- Ensure at least 100GB free space for build artifacts

