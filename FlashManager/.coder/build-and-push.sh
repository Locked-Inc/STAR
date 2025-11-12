#!/bin/bash
set -e

# Build and Push FlashManager Development Image
# This script builds the Docker image and optionally pushes it to a registry

IMAGE_NAME="${IMAGE_NAME:-flashmanager-dev}"
IMAGE_TAG="${IMAGE_TAG:-latest}"
REGISTRY="${REGISTRY:-}"  # Empty by default, set to your registry URL

echo "=================================================="
echo "Building FlashManager Development Image"
echo "=================================================="
echo ""
echo "Image: ${IMAGE_NAME}:${IMAGE_TAG}"
if [ -n "$REGISTRY" ]; then
    echo "Registry: ${REGISTRY}"
fi
echo ""

# Build the Docker image
echo "🔨 Building Docker image..."
docker build -t "${IMAGE_NAME}:${IMAGE_TAG}" -f Dockerfile .

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build complete!"
    echo ""
    echo "Image: ${IMAGE_NAME}:${IMAGE_TAG}"
    docker images | grep "${IMAGE_NAME}" | head -1
else
    echo ""
    echo "❌ Build failed!"
    exit 1
fi

# Tag and push to registry if specified
if [ -n "$REGISTRY" ]; then
    FULL_IMAGE="${REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}"

    echo ""
    echo "🏷️  Tagging image for registry..."
    docker tag "${IMAGE_NAME}:${IMAGE_TAG}" "${FULL_IMAGE}"

    echo ""
    echo "📤 Pushing to registry..."
    docker push "${FULL_IMAGE}"

    if [ $? -eq 0 ]; then
        echo ""
        echo "✅ Push complete!"
        echo ""
        echo "Registry image: ${FULL_IMAGE}"
        echo ""
        echo "Update main.tf to use this image:"
        echo ""
        echo "  resource \"docker_image\" \"flashmanager_dev\" {"
        echo "    name = \"${FULL_IMAGE}\""
        echo "  }"
    else
        echo ""
        echo "❌ Push failed!"
        exit 1
    fi
else
    echo ""
    echo "ℹ️  No registry specified - image built locally only"
    echo ""
    echo "To push to a registry, run:"
    echo "  REGISTRY=your-registry.com ./build-and-push.sh"
    echo ""
    echo "Or manually:"
    echo "  docker tag ${IMAGE_NAME}:${IMAGE_TAG} your-registry.com/${IMAGE_NAME}:${IMAGE_TAG}"
    echo "  docker push your-registry.com/${IMAGE_NAME}:${IMAGE_TAG}"
fi

echo ""
echo "=================================================="
echo ""
