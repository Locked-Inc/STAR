# FlashManager Coder Template
# Complete development environment for STAR embedded systems
FROM codercom/enterprise-base:ubuntu

USER root

# Install system dependencies
RUN apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    git \
    curl \
    wget \
    unzip \
    vim \
    nano \
    # ESP32 dependencies
    python3 \
    python3-pip \
    python3-venv \
    # Buildroot dependencies for Pi5
    libncurses5-dev \
    bc \
    rsync \
    cpio \
    # Android dependencies
    openjdk-17-jdk \
    # PostgreSQL client
    postgresql-client \
    # Other utilities
    tree \
    jq \
    htop \
    && rm -rf /var/lib/apt/lists/*

# Install Node.js 18.x
RUN curl -fsSL https://deb.nodesource.com/setup_18.x | bash - \
    && apt-get install -y nodejs \
    && npm install -g npm@latest

# Install ESP-IDF (ESP32 toolchain)
ENV IDF_PATH=/opt/esp-idf
ENV IDF_TOOLS_PATH=/opt/esp-tools

RUN git clone --recursive --depth 1 --branch v5.1.2 \
    https://github.com/espressif/esp-idf.git ${IDF_PATH} \
    && cd ${IDF_PATH} \
    && ./install.sh esp32 \
    && pip3 install esptool

# Install Android SDK Command Line Tools
ENV ANDROID_HOME=/opt/android-sdk
ENV PATH=${PATH}:${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools

RUN mkdir -p ${ANDROID_HOME}/cmdline-tools \
    && cd ${ANDROID_HOME}/cmdline-tools \
    && wget -q https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip \
    && unzip commandlinetools-linux-9477386_latest.zip \
    && mv cmdline-tools latest \
    && rm commandlinetools-linux-9477386_latest.zip \
    && yes | latest/bin/sdkmanager --licenses \
    && latest/bin/sdkmanager "platform-tools" "platforms;android-33" "build-tools;33.0.1"

# Install Rust for flash agent
ENV RUSTUP_HOME=/opt/rust
ENV CARGO_HOME=/opt/cargo
ENV PATH=${PATH}:${CARGO_HOME}/bin

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y \
    && ${CARGO_HOME}/bin/rustup default stable

# Set up Gradle
ENV GRADLE_HOME=/opt/gradle
ENV PATH=${PATH}:${GRADLE_HOME}/bin

RUN wget -q https://services.gradle.org/distributions/gradle-8.5-bin.zip \
    && unzip gradle-8.5-bin.zip -d /opt \
    && mv /opt/gradle-8.5 ${GRADLE_HOME} \
    && rm gradle-8.5-bin.zip

# Create workspace directory structure
RUN mkdir -p /workspace/STAR/ESP32 \
    && mkdir -p /workspace/STAR/Pi5 \
    && mkdir -p /workspace/STAR/Android \
    && mkdir -p /workspace/STAR/Backends \
    && mkdir -p /workspace/STAR/FlashManager/backend \
    && mkdir -p /workspace/STAR/FlashManager/frontend \
    && mkdir -p /workspace/STAR/FlashManager/agent \
    && mkdir -p /workspace/STAR/FlashManager/artifacts \
    && chown -R coder:coder /workspace

USER coder
WORKDIR /workspace

# Source ESP-IDF in bashrc
RUN echo '. ${IDF_PATH}/export.sh' >> ~/.bashrc

# Configure git
RUN git config --global user.email "coder@example.com" \
    && git config --global user.name "Coder"

# Install global npm packages
RUN npm install -g typescript vite @playwright/test

# Set environment variables
ENV WORKSPACE_DIR=/workspace
ENV STAR_DIR=/workspace/STAR
ENV FLASHMANAGER_DIR=/workspace/STAR/FlashManager

# Expose ports
# 8080 - Projectum backend (if running)
# 8081 - FlashManager backend
# 5173 - Projectum frontend
# 5174 - FlashManager frontend
# 5432 - PostgreSQL (Projectum)
# 5433 - PostgreSQL (FlashManager)
EXPOSE 8080 8081 5173 5174 5432 5433

CMD ["/bin/bash"]
