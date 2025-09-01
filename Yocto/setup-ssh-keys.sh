#!/bin/bash

# Setup SSH key authentication for passwordless access

echo "Setting up SSH key authentication..."

# Copy public key to VM using password authentication
echo "Copying SSH public key to VM..."
cat ~/.ssh/yocto-vm.pub | ssh -o StrictHostKeyChecking=no yoctouser@localhost -p 2222 \
  'mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys'

echo "SSH key authentication configured!"
echo "Testing passwordless connection..."

# Test the key
ssh -i ~/.ssh/yocto-vm -o StrictHostKeyChecking=no yoctouser@localhost -p 2222 \
  'echo "✓ Passwordless SSH working! Disk space: $(df -h / | tail -1 | awk '"'"'{print $4}'"'"' available)"'