#!/bin/bash
set -e

echo "Configuring SPI permissions for STAR Platform..."

# 1. Add current user to spi group
if getent group spi > /dev/null; then
    echo "Adding user $USER to spi group..."
    sudo usermod -a -G spi $USER
else
    echo "Group 'spi' does not exist. Creating it..."
    sudo groupadd spi
    sudo usermod -a -G spi $USER
fi

# 2. Create udev rule
echo "Creating udev rule for /dev/spidev*..."
echo 'SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"' | sudo tee /etc/udev/rules.d/50-spi.rules

echo "Done. Please reboot for changes to take effect."
