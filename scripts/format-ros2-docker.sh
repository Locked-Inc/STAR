#!/bin/bash
# Format ROS2 code using Docker/devcontainer
# This script runs the format-ros2.sh script inside the devcontainer

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check if we're already in a container
if [ -f "/.dockerenv" ] || grep -q docker /proc/1/cgroup 2>/dev/null; then
    # Already in container, run directly
    exec "$SCRIPT_DIR/format-ros2.sh" "$@"
fi

# Check if devcontainer CLI is available
if command -v devcontainer &> /dev/null; then
    echo "Running format-ros2.sh in devcontainer..."
    devcontainer exec --workspace-folder "$PROJECT_ROOT" /bin/bash -c "cd /workspaces/STAR && ./scripts/format-ros2.sh $*"
# Check if docker is available
elif command -v docker &> /dev/null; then
    echo "Running format-ros2.sh in Docker container..."
    
    # Find the running STAR container or start one
    CONTAINER_ID=$(docker ps --filter "label=devcontainer.local_folder=$PROJECT_ROOT" --format "{{.ID}}" | head -1)
    
    if [ -z "$CONTAINER_ID" ]; then
        echo "Error: No running devcontainer found for this project."
        echo ""
        echo "Options:"
        echo "1. Open the project in VS Code with Dev Containers extension"
        echo "2. Start the devcontainer manually"
        echo "3. Run formatting manually in a ROS2 environment:"
        echo "   cd star-ros2/src/<package> && ament_uncrustify --reformat"
        exit 1
    fi
    
    docker exec -it "$CONTAINER_ID" /bin/bash -c "cd /workspaces/STAR && ./scripts/format-ros2.sh $*"
else
    echo "Error: Neither devcontainer nor docker found."
    echo ""
    echo "To format ROS2 code, you need to either:"
    echo "1. Run this inside the devcontainer"
    echo "2. Have Docker installed to use the devcontainer"
    echo "3. Install ROS2 and run: ./scripts/format-ros2.sh"
    exit 1
fi
