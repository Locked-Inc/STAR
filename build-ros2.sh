#!/bin/bash
set -e

# Install system dependencies
apt-get update
apt-get install -y --no-install-recommends \
  libprotobuf-dev \
  protobuf-compiler \
  protobuf-compiler-grpc \
  libgrpc++-dev \
  libgrpc-dev

# Install buf
BUF_VERSION=1.28.1
curl -sSL "https://github.com/bufbuild/buf/releases/download/v${BUF_VERSION}/buf-$(uname -s)-$(uname -m)" -o /usr/local/bin/buf
chmod +x /usr/local/bin/buf

# Generate protos
cd /workspaces/STAR/star-proto
mkdir -p /tmp/star-proto
cp -r proto/star /tmp/star-proto/

cat > /tmp/star-proto/buf.yaml <<EOF
version: v1
lint:
  use: [DEFAULT]
EOF

cat > /tmp/buf.gen.ros2.yaml <<EOF
version: v1
plugins:
  - plugin: buf.build/protocolbuffers/cpp:v21.12
    out: /workspaces/STAR/star-proto/gen/cpp
  - plugin: buf.build/grpc/cpp:v1.51.1
    out: /workspaces/STAR/star-proto/gen/cpp
EOF

buf generate /tmp/star-proto --template /tmp/buf.gen.ros2.yaml

# Build ROS2
cd /workspaces/STAR/star-ros2
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
