#!/bin/bash

# This script sets up the build environment for creating a custom PYNQ image.

# Exit on error
set -e

# 1. Initialize repo
# This will download the Yocto layers specified in the manifest.xml file.
if [ ! -d ".repo" ]; then
  repo init -u . -m manifest.xml --no-repo-verify
fi

# 2. Sync the repositories
# This will download the source code for all the layers.
repo sync

# 3. Source the build environment setup script
# This sets up the environment variables for the Yocto build.
# Note: This needs to be done in the shell where you will run the build.
# This script just checks that the file exists.
if [ -f "sources/poky/oe-init-build-env" ]; then
  echo "Build environment setup complete."
  echo "Run 'source sources/poky/oe-init-build-env' to start the build environment."
else
  echo "Error: sources/poky/oe-init-build-env not found."
  echo "Please run './setup_build_env.sh' again."
  exit 1
fi