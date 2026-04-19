#!/usr/bin/env bash
# Install the Hailo-8L runtime stack on a Pi 5.
#
# Supports two target OSes:
#   - Raspberry Pi OS Bookworm 64-bit (Python 3.11): apt install hailo-all
#     is sufficient; `hailo_platform` imports directly under the system
#     python and the perception node uses the in-process HailoRT path.
#   - Ubuntu 24.04 Noble (Python 3.12): the apt `hailo-all` meta-package
#     is not available and the cp311 PyHailoRT wheel is ABI-incompatible
#     with the system python. We install the driver + firmware + CLI
#     runtime from Hailo's apt repo, then build a Python 3.11 venv at
#     /home/star/hailo-venv and install `python3-hailort` into it so the
#     perception node can reach HailoRT via its subprocess-bridge path.
#
# Re-run after swapping the HAT+ or upgrading the host OS.

set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "Re-running with sudo..."
  exec sudo --preserve-env=HOME "$0" "$@"
fi

# -- detect OS --
. /etc/os-release
case "${ID}:${VERSION_CODENAME:-}" in
  raspbian:bookworm|debian:bookworm)
    OS_KIND="pi-os-bookworm"
    ;;
  ubuntu:noble)
    OS_KIND="ubuntu-noble"
    ;;
  *)
    echo "Unsupported host: ${PRETTY_NAME}."
    echo "Supported: Raspberry Pi OS Bookworm, Ubuntu 24.04 Noble."
    exit 1
    ;;
esac
echo "Target OS: ${OS_KIND}"

echo "[1/6] apt update"
apt update

if [[ "${OS_KIND}" == "pi-os-bookworm" ]]; then
  echo "[2/6] Installing hailo-all (driver + firmware + HailoRT + TAPPAS)"
  apt install -y hailo-all
else
  echo "[2/6] Installing hailo-dkms + hailofw + hailort + hailortcli"
  # These are the standalone packages published by Hailo's apt repo;
  # hailo-all is Debian-bookworm-only because it pulls tappas deps that
  # need libav5.1 from bookworm.
  apt install -y hailo-dkms hailofw hailort
fi

echo "[3/6] Confirming PCIe overlay is enabled"
# config.txt lives in /boot/firmware on both Pi OS Bookworm and Ubuntu 24.04.
CONFIG=/boot/firmware/config.txt
if [[ -f "$CONFIG" ]]; then
  if ! grep -q "^dtparam=pciex1" "$CONFIG"; then
    echo "dtparam=pciex1" >> "$CONFIG"
    echo "  added dtparam=pciex1"
  fi
  if ! grep -q "^dtoverlay=pciex1-compat-pi5" "$CONFIG"; then
    echo "dtoverlay=pciex1-compat-pi5,no-mip" >> "$CONFIG"
    echo "  added dtoverlay=pciex1-compat-pi5,no-mip"
  fi
else
  echo "  $CONFIG not present (non-standard image); skipping overlay edit"
fi

echo "[4/6] Verifying device file"
if [[ -e /dev/hailo0 ]]; then
  echo "  /dev/hailo0 present"
else
  echo "  /dev/hailo0 not present yet -- will appear after reboot"
fi

if [[ "${OS_KIND}" == "ubuntu-noble" ]]; then
  echo "[5/6] Building Python 3.11 venv for HailoRT Python bindings"
  # Python 3.11 isn't in Ubuntu 24.04's main repos; enable deadsnakes if
  # not already installed.
  if ! command -v python3.11 >/dev/null; then
    add-apt-repository -y ppa:deadsnakes/ppa
    apt update
    apt install -y python3.11 python3.11-venv python3.11-dev
  fi

  VENV=/home/star/hailo-venv
  if [[ ! -x "${VENV}/bin/python3.11" ]]; then
    sudo -u star python3.11 -m venv "${VENV}"
  fi
  # numpy<2 is REQUIRED: the cp311 _pyhailort.so is compiled against
  # numpy 1.x ABI. numpy >= 2 fails inference with a misleading
  # "Memory size of vstream ... does not match the frame count" error.
  sudo -u star "${VENV}/bin/pip" install --upgrade pip wheel
  sudo -u star "${VENV}/bin/pip" install 'numpy<2' netaddr contextlib2 argcomplete future

  # Extract hailo_platform/ from the cp311 deb into the venv.
  DEB_PATH=/tmp/python3-hailort.deb
  if [[ ! -f "${DEB_PATH}" ]]; then
    apt download python3-hailort || {
      echo "  python3-hailort not available via apt download; ensure"
      echo "  Hailo's apt repo is configured in /etc/apt/sources.list.d/."
      exit 1
    }
    mv python3-hailort_*.deb "${DEB_PATH}"
  fi
  WORK=$(mktemp -d)
  dpkg-deb -x "${DEB_PATH}" "${WORK}"
  # The deb drops hailo_platform/ under usr/lib/python3/dist-packages/.
  SRC="${WORK}/usr/lib/python3/dist-packages/hailo_platform"
  if [[ -d "${SRC}" ]]; then
    DST="${VENV}/lib/python3.11/site-packages/hailo_platform"
    rm -rf "${DST}"
    cp -r "${SRC}" "${DST}"
    chown -R star:star "${DST}"
    echo "  hailo_platform installed into ${DST}"
  else
    echo "  hailo_platform not found in deb layout — check package version" >&2
    exit 1
  fi
  rm -rf "${WORK}"

  sudo -u star "${VENV}/bin/python" -c "import hailo_platform; print('venv import OK:', hailo_platform.__file__)" || {
    echo "  venv import of hailo_platform failed." >&2
    exit 1
  }
else
  echo "[5/6] Pi OS Bookworm: hailo_platform available from system python, no venv needed"
fi

echo "[6/6] Done. Next steps:"
echo "  1. Reboot (sudo reboot) to load the kernel driver if /dev/hailo0"
echo "     didn't show up in step 4."
echo "  2. Verify: hailortcli fw-control identify"
echo "     Expected: Device Architecture: HAILO8L"
echo "  3. Download the YOLOv8s model:"
echo "     bash $(dirname "$0")/download_yolov8s_hef.sh"
echo
read -r -p "Reboot now? [y/N] " ans
if [[ "$ans" =~ ^[Yy]$ ]]; then
  reboot
fi
