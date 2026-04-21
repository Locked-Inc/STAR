#!/usr/bin/env bash
#
# devcontainer-exec.sh -- run a command inside the STAR dev container.
#
# Used by hal-audit Claude instances on macOS where the host Mac blocks
# the GNURX cross-compiler (Apple "could not verify rx-elf-gcc"). All
# build / cross-compile commands MUST run inside the dev container which
# has the toolchain and Linux kernel that GNURX expects.
#
# Usage from inside a Claude instance:
#   bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"
#
# Or for one-off commands:
#   bash scripts/hal-audit/devcontainer-exec.sh "make -C star-rx72n-firmware/uart_test"
#
# First run will spin the container up if it isn't already running.

set -euo pipefail

REPO_DIR=$(git rev-parse --show-toplevel)
CMD="${1:-}"

if [ -z "$CMD" ]; then
    echo "Usage: $0 \"<shell command to run inside the dev container>\"" >&2
    exit 1
fi

# Ensure devcontainer CLI is installed
if ! command -v devcontainer >/dev/null 2>&1; then
    cat >&2 <<'EOF'
ERROR: `devcontainer` CLI not found.
Install with:  npm install -g @devcontainers/cli
(Requires Node.js + Docker Desktop running.)
EOF
    exit 1
fi

# Bring the container up if it isn't running yet (idempotent + cached).
if ! devcontainer exec --workspace-folder "$REPO_DIR" true 2>/dev/null; then
    echo "==> Bringing dev container up (first run, may take a few minutes)..." >&2
    DOCKER_BUILDKIT=1 devcontainer up --workspace-folder "$REPO_DIR" >&2
fi

# Run the actual command inside the container.
exec devcontainer exec --workspace-folder "$REPO_DIR" bash -lc "$CMD"
