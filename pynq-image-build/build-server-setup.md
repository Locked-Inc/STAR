# Build Server Setup

This document outlines the necessary steps to configure a fresh Ubuntu 22.04 server to act as a Yocto build host for PYNQ.

## 1. Install Essential Dependencies

These packages are required for the Yocto build system.

```bash
sudo apt-get update
sudo apt-get install -y gawk wget git diffstat unzip texinfo gcc build-essential chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3 xterm python3-subunit mesa-common-dev zstd liblz4-tool repo
```

## 2. Install Google Repo

The `repo` tool is used to manage the multiple git repositories that make up the Yocto Project sources.

```bash
sudo apt-get install -y repo
```

## 3. Configure Git

The Yocto build system uses git to fetch source code. You need to configure your git identity.

```bash
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

## 4. (Optional) Install Xilinx Tools

While the Yocto build can generate the entire software stack, you may need the Xilinx tools (Vivado, Vitis) for hardware design and creating the initial hardware platform (XSA file). If you are only building the software, you can skip this step.

If needed, install the Xilinx tools (version 2024.1 or later) to a location like `/tools/Xilinx`.
