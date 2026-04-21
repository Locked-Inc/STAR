#!/usr/bin/env bash
#
# broadcast.sh -- send a follow-up message to every Claude instance in the
#                 hal-audit tmux session. Useful for mid-flight corrections
#                 (e.g. "use the dev container for builds").
#
# Usage:
#   bash scripts/hal-audit/broadcast.sh "Your message here"
#   bash scripts/hal-audit/broadcast.sh --devcontainer   # canned message
#

set -euo pipefail

SESSION="${HAL_AUDIT_SESSION:-hal-audit}"

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 \"<message to send to every Claude instance>\""
    echo "       $0 --devcontainer   (canned dev-container correction)"
    exit 1
fi

case "${1:-}" in
    --devcontainer)
        MSG='IMPORTANT CORRECTION: rx-elf-gcc is blocked by macOS gatekeeper on the host. Do NOT call rx-elf-gcc, make, or bash build.sh directly. For ALL build / cross-compile commands, run them inside the dev container by wrapping each command with: bash scripts/hal-audit/devcontainer-exec.sh "<your command>". Example: bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh". The helper auto-spins the container the first time. After all your edits, do this build verification through the helper before committing and pushing.'
        ;;
    *)
        MSG="$1"
        ;;
esac

if ! tmux has-session -t "$SESSION" 2>/dev/null; then
    echo "tmux session '$SESSION' not running" >&2
    exit 1
fi

WINDOWS=$(tmux list-windows -t "$SESSION" -F '#I')
N=$(echo "$WINDOWS" | wc -l | tr -d ' ')
echo "broadcasting to $N windows in session '$SESSION'..."

for W in $WINDOWS; do
    NAME=$(tmux list-windows -t "$SESSION" -F '#I:#W' | grep "^$W:" | cut -d: -f2)
    echo "  -> [$NAME]"
    # Send the message text, then Enter to submit it to the Claude prompt.
    tmux send-keys -t "$SESSION:$W" "$MSG" Enter
done

echo "done. check each window for the new instruction landing in Claude's prompt."
