#!/bin/bash

# Script to transfer setup script to VM and execute it

echo "Transferring setup script to VM..."
scp -P 2222 -o StrictHostKeyChecking=no setup-yocto-j721e.sh yoctouser@localhost:/home/yoctouser/

echo "Making script executable and running setup..."
ssh -p 2222 -o StrictHostKeyChecking=no yoctouser@localhost << 'EOF'
chmod +x /home/yoctouser/setup-yocto-j721e.sh
echo "Starting Yocto setup for J721E TDA4VM..."
/home/yoctouser/setup-yocto-j721e.sh
EOF

echo "Setup complete! You can now SSH into the VM and build Yocto images."