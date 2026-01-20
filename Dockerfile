FROM osrf/ros:jazzy-desktop

# Set up environment
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive

# Install essential tools and dependencies
RUN apt-get update && apt-get install -y \
    python3-pip \
    python3-colcon-common-extensions \
    python3-rosdep \
    libgpiod-dev \
    i2c-tools \
    v4l-utils \
    udev \
    sudo \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libgrpc-dev \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install buf
ARG BUF_VERSION=1.28.1
RUN curl -sSL "https://github.com/bufbuild/buf/releases/download/v${BUF_VERSION}/buf-$(uname -s)-$(uname -m)" -o /usr/local/bin/buf \
    && chmod +x /usr/local/bin/buf

# Set up user
ARG USERNAME=star
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN if getent passwd $USER_UID >/dev/null; then \
        EXISTING_USER=$(getent passwd $USER_UID | cut -d: -f1); \
        if [ "$EXISTING_USER" != "$USERNAME" ]; then \
            usermod -l $USERNAME $EXISTING_USER; \
            groupmod -n $USERNAME $(getent group $USER_GID | cut -d: -f1); \
            usermod -d /home/$USERNAME -m $USERNAME; \
        fi; \
    else \
        groupadd --gid $USER_GID $USERNAME; \
        useradd --uid $USER_UID --gid $USER_GID -m $USERNAME; \
    fi

# NOTE: Granting NOPASSWD sudo access to all commands is a security risk.
# This is intended only for development inside this container. Do NOT use
# this pattern in production; instead, restrict sudo to specific commands
# (e.g., usermod, rosdep) as needed.
RUN echo "$USERNAME ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Add user to groups for hardware access
# Create spi and i2c groups if they don't exist, then add user
RUN groupadd -f spi && groupadd -f i2c \
    && usermod -aG video,dialout,plugdev,spi,i2c $USERNAME

# Set up workspace
WORKDIR /workspaces/STAR

# Source ROS2 setup in bashrc
RUN echo "source /opt/ros/jazzy/setup.bash" >> /home/$USERNAME/.bashrc

USER $USERNAME
