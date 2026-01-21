#!/bin/bash
# Format ROS2 code using Docker (standalone, doesn't require devcontainer)
# Usage: ./scripts/format-ros2-standalone.sh [--check]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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
docker build -t star-ros2-dev "$PROJECT_ROOT"

# Run formatting in Docker
echo "📝 Running ROS2 formatting in Docker..."
docker run --rm \
    -v "$PROJECT_ROOT:/workspaces/STAR" \
    -w /workspaces/STAR \
    star-ros2-dev \
    /bin/bash -c "cd /workspaces/STAR && ./scripts/format-ros2.sh $CHECK_ONLY"

echo "✅ Done!"
