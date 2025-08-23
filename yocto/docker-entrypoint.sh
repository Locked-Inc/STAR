#!/bin/bash

# Docker entrypoint script for Yocto build environment

echo "=========================================="
echo "   TDA4VM Yocto Build Environment"
echo "=========================================="
echo "SDK Version: TI Processor SDK Analytics 11.01.07.05"
echo "Target: J721E EVM (TDA4VM)"
echo "Yocto Version: Scarthgap"
echo ""

# Check if we're in the yocto directory
if [ -d "/home/yocto/yocto-workspace/build" ]; then
    echo "✅ Yocto environment detected"
    echo "🔧 Setting up build environment..."
    
    cd /home/yocto/yocto-workspace/build
    
    # Source the environment if it exists
    if [ -f "conf/setenv" ]; then
        echo "📋 Sourcing Yocto environment..."
        . conf/setenv
        echo "🎯 Ready to build! Available commands:"
        echo ""
        echo "  make build          - Build tisdk-edgeai-image for J721E"
        echo "  make build-minimal  - Build minimal image for testing"
        echo "  make clean          - Clean build artifacts"
        echo "  make menuconfig     - Configure kernel"
        echo "  make shell          - Interactive build shell"
        echo ""
        echo "📁 Build directory: $(pwd)"
        echo "🏗️  To build EdgeAI image: MACHINE=j721e-evm bitbake tisdk-edgeai-image"
        echo ""
    else
        echo "⚠️  Yocto environment not set up. Run 'make setup' first."
    fi
else
    echo "⚠️  Yocto workspace not found. Mount your yocto directory to /home/yocto/yocto-workspace"
fi

echo "=========================================="
echo ""

# Execute the command
exec "$@"