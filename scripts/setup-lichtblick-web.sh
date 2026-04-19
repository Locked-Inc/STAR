#!/usr/bin/env bash
# One-time setup: download Lichtblick web app for offline self-hosting.
#
# Lichtblick is the open-source fork of Foxglove Studio. It publishes a
# static web bundle compatible with the foxglove_bridge WebSocket protocol.
#
# Run this ONCE while the Pi5 has internet access (e.g. on TAMU_VISITOR or
# Ethernet). After setup, start.sh serves Lichtblick at http://192.168.50.1:8081
# so your laptop can use it with no internet required.
#
# Usage:
#   ./scripts/setup-lichtblick-web.sh              # install latest pinned version
#   ./scripts/setup-lichtblick-web.sh --version 1.24.3

set -euo pipefail

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LICHTBLICK_VERSION="1.24.3"
LICHTBLICK_WEB_DIR="/opt/star-lichtblick-web"
LAYOUT_SRC="$STAR_DIR/star-ros2/src/star_bringup/foxglove/star-default.json"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
say()  { echo -e "${GREEN}[lichtblick]${NC} $*"; }
warn() { echo -e "${YELLOW}[lichtblick]${NC} $*"; }
die()  { echo -e "${RED}[lichtblick] ERROR:${NC} $*" >&2; exit 1; }

for arg in "$@"; do
    case "$arg" in
        --version) shift; LICHTBLICK_VERSION="$1"; shift ;;
        --version=*) LICHTBLICK_VERSION="${arg#--version=}" ;;
    esac
done

DOWNLOAD_URL="https://github.com/lichtblick-suite/lichtblick/releases/download/v${LICHTBLICK_VERSION}/lichtblick-web.tar.gz"

say "Lichtblick v${LICHTBLICK_VERSION} -- static web bundle setup"
say "Install dir : $LICHTBLICK_WEB_DIR"
say "Served on   : http://192.168.50.1:8081 (when AP is active)"
echo ""

# Verify internet is reachable before starting
if ! curl -fsS --max-time 5 https://github.com >/dev/null 2>&1; then
    die "No internet access -- connect Pi5 to WiFi or Ethernet first, then re-run"
fi

say "Downloading lichtblick-web.tar.gz (~50 MB)..."
curl -fsSL --progress-bar "$DOWNLOAD_URL" -o /tmp/lichtblick-web.tar.gz \
    || die "Download failed from $DOWNLOAD_URL"

say "Extracting to $LICHTBLICK_WEB_DIR..."
sudo rm -rf "$LICHTBLICK_WEB_DIR"
sudo mkdir -p "$LICHTBLICK_WEB_DIR"
sudo tar -xzf /tmp/lichtblick-web.tar.gz -C "$LICHTBLICK_WEB_DIR" --strip-components=1
rm -f /tmp/lichtblick-web.tar.gz

# Copy the STAR default layout so users can import it via URL
# http://192.168.50.1:8081/star-layout.json
if [[ -f "$LAYOUT_SRC" ]]; then
    sudo cp "$LAYOUT_SRC" "$LICHTBLICK_WEB_DIR/star-layout.json"
    say "STAR layout available at http://192.168.50.1:8081/star-layout.json"
else
    warn "Layout not found at $LAYOUT_SRC -- skipping"
fi

sudo chown -R root:root "$LICHTBLICK_WEB_DIR"

# Verify the install looks correct
if [[ ! -f "$LICHTBLICK_WEB_DIR/index.html" ]]; then
    die "index.html not found in $LICHTBLICK_WEB_DIR -- tarball structure may have changed"
fi

say "Setup complete."
echo ""
echo "  Next steps:"
echo "    1. Run ./start.sh  -- Lichtblick served at http://192.168.50.1:8081"
echo "    2. On your Mac: join STAR-Robot WiFi, open http://192.168.50.1:8081"
echo "    3. Connect Lichtblick to ws://192.168.50.1:8765"
echo "    4. Import layout: http://192.168.50.1:8081/star-layout.json"
echo ""
echo "  Mac desktop app (alternative):"
echo "    https://github.com/lichtblick-suite/lichtblick/releases/download/v${LICHTBLICK_VERSION}/lichtblick-${LICHTBLICK_VERSION}-mac-universal.dmg"
