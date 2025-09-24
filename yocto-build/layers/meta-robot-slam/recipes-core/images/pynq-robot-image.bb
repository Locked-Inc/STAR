# PYNQ-Z2 Robot System Image Recipe
# Complete image for LiDAR SLAM and Computer Vision robot

DESCRIPTION = "Complete Linux image for PYNQ-Z2 based LiDAR SLAM and Computer Vision robot system"
LICENSE = "MIT"

# Inherit core image functionality
inherit core-image

# Image features
IMAGE_FEATURES += " \
    ssh-server-openssh \
    tools-debug \
    tools-profile \
    debug-tweaks \
    package-management \
    hwcodecs \
    x11-base \
"

# Base system packages
IMAGE_INSTALL = " \
    packagegroup-core-boot \
    packagegroup-core-ssh-openssh \
    ${CORE_IMAGE_EXTRA_INSTALL} \
"

# System utilities
IMAGE_INSTALL += " \
    kernel-modules \
    systemd \
    systemd-analyze \
    systemd-bootchart \
    udev-extraconf \
    e2fsprogs-resize2fs \
    parted \
    util-linux \
    dosfstools \
    htop \
    iotop \
    nano \
    vim \
    screen \
    tmux \
    rsync \
    wget \
    curl \
    git \
    cmake \
    make \
    gcc \
    g++ \
    gdb \
    strace \
    ltrace \
"

# Networking
IMAGE_INSTALL += " \
    openssh \
    openssh-sftp-server \
    dhcp-client \
    wpa-supplicant \
    iw \
    wireless-regdb \
    iperf3 \
    tcpdump \
    netcat \
    bridge-utils \
    iptables \
    ntp \
    chrony \
"

# Python environment
IMAGE_INSTALL += " \
    python3 \
    python3-modules \
    python3-pip \
    python3-setuptools \
    python3-numpy \
    python3-scipy \
    python3-matplotlib \
    python3-pillow \
    python3-requests \
    python3-flask \
    python3-tornado \
    python3-zmq \
    python3-serial \
    python3-smbus \
    python3-opencv \
    python3-pandas \
"

# PYNQ specific packages
IMAGE_INSTALL += " \
    pynq-framework \
    jupyter-notebook \
    python3-jupyter \
    python3-notebook \
    python3-ipykernel \
    python3-ipywidgets \
"

# ROS2 packages
IMAGE_INSTALL += " \
    ros-core \
    ros-base \
    ros2-navigation \
    ros2-slam-toolbox \
    ros2-cartographer \
    ros2-image-transport \
    ros2-cv-bridge \
    ros2-stereo-image-proc \
    ros2-lidar-drivers \
    ros2-sensor-msgs \
    ros2-geometry-msgs \
    ros2-tf2 \
    ros2-robot-state-publisher \
"

# Computer vision packages
IMAGE_INSTALL += " \
    opencv \
    opencv-apps \
    opencv-samples \
    v4l-utils \
    media-ctl \
    gstreamer1.0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    ffmpeg \
"

# Hardware interfaces
IMAGE_INSTALL += " \
    i2c-tools \
    spi-tools \
    can-utils \
    gpio-utils \
    usbutils \
    pciutils \
    ethtool \
    minicom \
"

# Machine learning and AI
IMAGE_INSTALL += " \
    vitis-ai-runtime \
    tensorflow-lite \
    onnxruntime \
    python3-onnx \
    python3-tflite-runtime \
    opencl-headers \
    opencl-icd-loader \
"

# FPGA and Xilinx tools
IMAGE_INSTALL += " \
    xrt \
    zocl \
    fpga-manager-script \
    device-tree-compiler \
"

# Robot-specific packages
IMAGE_INSTALL += " \
    robot-gateway-bridge \
    lidar-slam-node \
    stereo-camera-node \
    sensor-fusion-node \
    robot-control-service \
"

# Development tools
IMAGE_INSTALL += " \
    binutils \
    file \
    findutils \
    less \
    tree \
    which \
    procps \
    psmisc \
    shadow \
"

# Additional filesystem support
IMAGE_INSTALL += " \
    nfs-utils \
    sshfs-fuse \
    ntfs-3g \
"

# Set root filesystem size (8GB SD card)
IMAGE_ROOTFS_EXTRA_SPACE = "2000000"
IMAGE_ROOTFS_MAXSIZE = "6000000"

# Enable systemd
DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"

# X11 and display support
DISTRO_FEATURES:append = " x11"

# Enable hardware features
DISTRO_FEATURES:append = " opengl opencl wifi bluetooth"

# License management
LICENSE_FLAGS_ACCEPTED = "commercial synaptics-killswitch"

# Boot configuration
SERIAL_CONSOLES = "115200;ttyPS0"

# Custom post-install scripts
ROOTFS_POSTPROCESS_COMMAND += "robot_image_postprocess; "

robot_image_postprocess() {
    # Create robot-specific directories
    install -d ${IMAGE_ROOTFS}/opt/robot
    install -d ${IMAGE_ROOTFS}/opt/robot/logs
    install -d ${IMAGE_ROOTFS}/opt/robot/config
    install -d ${IMAGE_ROOTFS}/opt/robot/data
    install -d ${IMAGE_ROOTFS}/home/root/notebooks
    
    # Set permissions
    chmod 755 ${IMAGE_ROOTFS}/opt/robot
    chmod 755 ${IMAGE_ROOTFS}/home/root/notebooks
    
    # Create symbolic links for convenience
    ln -sf /opt/robot ${IMAGE_ROOTFS}/home/root/robot
    
    # Set up systemd services
    install -d ${IMAGE_ROOTFS}/etc/systemd/system/multi-user.target.wants
    
    # Enable services
    ln -sf /lib/systemd/system/robot-gateway.service \
        ${IMAGE_ROOTFS}/etc/systemd/system/multi-user.target.wants/robot-gateway.service
    ln -sf /lib/systemd/system/jupyter-notebook.service \
        ${IMAGE_ROOTFS}/etc/systemd/system/multi-user.target.wants/jupyter-notebook.service
}