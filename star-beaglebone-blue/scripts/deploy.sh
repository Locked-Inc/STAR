#!/usr/bin/env bash
# deploy.sh -- Deploy star-beaglebone-blue firmware to BeagleBone Blue via SSH
#
# Usage:
#   ./scripts/deploy.sh [--build] [--run] [--host <ip>] [--user <user>]
#
# The RPi5 connects to the BBB via USB CDC ethernet:
#   BBB usb0: 192.168.7.2  (RNDIS)
#   BBB usb1: 192.168.6.2  (CDC ECM)
#
# Prerequisites:
#   - BBB reachable via SSH (default: debian@192.168.6.2)
#   - sshpass installed on deploy host (or SSH keys configured)
#   - gcc >= 13 on BBB (Debian Trixie or newer)
#   - librobotcontrol installed on BBB

set -euo pipefail

# Defaults
BBB_HOST="${BBB_HOST:-192.168.6.2}"
BBB_USER="${BBB_USER:-debian}"
BBB_PASS="${BBB_PASS:-temppwd}"
BBB_DIR="/home/${BBB_USER}/star-beaglebone-blue"
DO_BUILD=false
DO_RUN=false

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)  DO_BUILD=true; shift ;;
        --run)    DO_RUN=true; shift ;;
        --host)   BBB_HOST="$2"; shift 2 ;;
        --user)   BBB_USER="$2"; shift 2 ;;
        *)        echo "Unknown arg: $1"; exit 1 ;;
    esac
done

SSH_CMD="sshpass -p ${BBB_PASS} ssh -o StrictHostKeyChecking=no ${BBB_USER}@${BBB_HOST}"
SCP_CMD="sshpass -p ${BBB_PASS} scp -o StrictHostKeyChecking=no"

echo "=== Deploying to ${BBB_USER}@${BBB_HOST} ==="

# Step 1: Sync source files
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"

echo "[1/4] Syncing source files..."
sshpass -p "${BBB_PASS}" rsync -az --delete \
    --exclude 'build-*' \
    --exclude '.git' \
    --exclude '*.o' \
    --exclude 'compile_commands.json' \
    -e "ssh -o StrictHostKeyChecking=no" \
    "${PROJECT_DIR}/" \
    "${BBB_USER}@${BBB_HOST}:${BBB_DIR}/"

# Step 2: Sync nanopb generated sources
echo "[2/4] Syncing nanopb generated sources..."
PROTO_GEN="${PROJECT_DIR}/../star-proto/gen/nanopb"
if [ -d "${PROTO_GEN}" ]; then
    sshpass -p "${BBB_PASS}" rsync -az \
        -e "ssh -o StrictHostKeyChecking=no" \
        "${PROTO_GEN}/" \
        "${BBB_USER}@${BBB_HOST}:${BBB_DIR}/../star-proto/gen/nanopb/"
fi

# Step 3: Build on device
if [ "${DO_BUILD}" = true ]; then
    echo "[3/4] Building on device..."
    ${SSH_CMD} "cd ${BBB_DIR} && \
        cmake -B build-native -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5 && \
        cmake --build build-native -j2 2>&1 | tail -10 && \
        echo 'BUILD OK' || echo 'BUILD FAILED'"
else
    echo "[3/4] Skipping build (use --build to build)"
fi

# Step 4: Run firmware
if [ "${DO_RUN}" = true ]; then
    echo "[4/4] Running firmware..."
    ${SSH_CMD} "echo ${BBB_PASS} | sudo -S ${BBB_DIR}/build-native/star-beaglebone-blue"
else
    echo "[4/4] Skipping run (use --run to execute)"
fi

echo "=== Deploy complete ==="
