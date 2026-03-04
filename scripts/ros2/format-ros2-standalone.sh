#!/usr/bin/env bash
# Format ROS2 code using Docker (standalone, doesn't require devcontainer)
# Usage: ./scripts/ros2/format-ros2-standalone.sh [--check]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo "❌ Docker is not installed or not in PATH"
    exit 1
fi

# Parse arguments
CHECK_ONLY=""
if [ "${1:-}" = "--check" ]; then
    CHECK_ONLY="--check"
fi

# Build the Docker image if needed
echo "🔨 Building star-ros2-dev Docker image..."
if [ ! -f "$PROJECT_ROOT/Dockerfile" ]; then
    echo "❌ Dockerfile not found in $PROJECT_ROOT"
    exit 1
fi
docker build -t star-ros2-dev "$PROJECT_ROOT"

# Preflight check
if [ ! -x "$PROJECT_ROOT/scripts/ros2/format-ros2.sh" ]; then
    echo "❌ scripts/ros2/format-ros2.sh not found or not executable"
    exit 1
fi

# Error trap
on_error() {
    local exit_code=$1
    echo "❌ Command failed with exit code $exit_code"
    echo "Context: CHECK_ONLY=$CHECK_ONLY PROJECT_ROOT=$PROJECT_ROOT"
    exit "$exit_code"
}
trap 'on_error $?' ERR

# Run formatting in Docker
echo "📝 Running ROS2 formatting in Docker..."
docker run --rm \
    -v "$PROJECT_ROOT:/workspaces/STAR" \
    -w /workspaces/STAR \
    star-ros2-dev \
    /workspaces/STAR/scripts/ros2/format-ros2.sh ${CHECK_ONLY:+"$CHECK_ONLY"}

echo "✅ Done!"
