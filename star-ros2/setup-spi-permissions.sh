#!/bin/bash
set -e

echo "Configuring SPI permissions for STAR Platform..."

# 1. Add current user (or specified user) to spi group
TARGET_USER=${1:-$USER}

if getent group spi > /dev/null; then
    echo "Adding user $TARGET_USER to spi group..."
    sudo usermod -a -G spi $TARGET_USER
else
    echo "Group 'spi' does not exist. Creating it..."
    sudo groupadd spi
    sudo usermod -a -G spi $TARGET_USER
fi

# 2. Create udev rule
echo "Creating udev rule for /dev/spidev*..."
echo 'SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"' | sudo tee /etc/udev/rules.d/50-spi.rules

echo "Done. Please reboot for changes to take effect."
