#!/bin/bash
# Script to build and run ROS2 in Docker container

set -e

cd "$(dirname "$0")"

# Build the Docker image
echo "Building Docker image..."
docker build -t star-ros2-dev .

# Run the build script inside the container
echo "Running ROS2 build in container..."
docker run --rm \
  -v "$(pwd):/workspaces/STAR" \
  -w /workspaces/STAR \
  star-ros2-dev \
  /bin/bash -c "cd /workspaces/STAR && ./build-ros2.sh"

echo "Build complete!"
