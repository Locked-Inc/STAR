#!/bin/bash
# Robot Control Package Installation Script
# Installs motor control and sensor integration packages

set -e

echo "Installing Robot Control packages..."

# Update package lists
apt-get update

# Install Java for Robot Gateway (Spring Boot)
apt-get install -y \
    openjdk-11-jre-headless \
    openjdk-11-jdk-headless

# Install Python control libraries
pip3 install --no-cache-dir \
    pyserial \
    smbus2 \
    RPi.GPIO || echo "RPi.GPIO skipped (PYNQ uses different GPIO interface)"

# Install PYNQ overlays for motor control
echo "Motor control will use PYNQ overlays and custom IP cores"

echo "Robot Control packages installed successfully"
