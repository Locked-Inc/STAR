#!/bin/bash
# Network Communication Package Installation Script
# Installs network utilities and communication libraries

set -e

echo "Installing Network Communication packages..."

# Update package lists
apt-get update

# Install network management tools
apt-get install -y \
    network-manager \
    wireless-tools \
    wpasupplicant \
    hostapd \
    dnsmasq

# Install Python networking libraries
pip3 install --no-cache-dir \
    flask \
    flask-cors \
    websockets \
    requests

echo "Network Communication packages installed successfully"
