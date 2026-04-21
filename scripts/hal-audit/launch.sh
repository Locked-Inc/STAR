#!/usr/bin/env bash
#
# launch.sh -- spin up N parallel Claude Sonnet instances in tmux to audit
#              the RX72N HAL register definitions against the HW manual.
#
# Each task gets:
#   - a fresh git worktree under $WORKTREE_BASE
#   - a branch named $BRANCH_PREFIX<task-id>
#   - one tmux window running `claude` with that task's prompt
#
# Tasks are auto-discovered from prompts/*.md.
#
# Requires:
#   - tmux
#   - git
#   - claude (https://docs.anthropic.com/en/docs/claude-code)
#
# Usage (from repo root):
#   bash scripts/hal-audit/launch.sh           # start all tasks
#   bash scripts/hal-audit/launch.sh --dry-run # show what would run
#   bash scripts/hal-audit/launch.sh --clean   # tear down worktrees + session
#
# Then: tmux attach -t hal-audit
#       Ctrl-b w  -- window list (one per task)
#       Ctrl-b n  -- next window
#       Ctrl-b d  -- detach
#

set -euo pipefail

REPO_DIR=$(git rev-parse --show-toplevel)
SCRIPT_DIR="$REPO_DIR/scripts/hal-audit"
PROMPT_DIR="$SCRIPT_DIR/prompts"
WORKTREE_BASE="${HAL_AUDIT_WORKTREE_BASE:-$HOME/star-hal-audit-worktrees}"
TMUX_SESSION="${HAL_AUDIT_SESSION:-hal-audit}"
BRANCH_PREFIX="${HAL_AUDIT_BRANCH_PREFIX:-bsikar/verifying-}"
BASE_REF="${HAL_AUDIT_BASE_REF:-main}"
CLAUDE_BIN="${CLAUDE_BIN:-claude}"

# Default: Sonnet 4.6, extended thinking enabled, max effort,
# bypassed permission prompts (so each instance can edit/commit/push
# without user interaction).
#
# Verified flag names (claude CLI):
#   --model claude-sonnet-4-6
#   --thinking enabled               (choices: enabled / adaptive / disabled)
#   --effort max                     (choices: low / medium / high / xhigh / max)
#   --dangerously-skip-permissions
#
# Override via env if needed:
#   CLAUDE_FLAGS='...' bash scripts/hal-audit/launch.sh
CLAUDE_FLAGS="${CLAUDE_FLAGS:---model claude-sonnet-4-6 --thinking enabled --effort max --dangerously-skip-permissions}"

DRY_RUN=0
CLEAN_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        --clean)   CLEAN_ONLY=1 ;;
        --help|-h)
            sed -n '3,30p' "$0"; exit 0 ;;
        *)
            echo "Unknown arg: $arg (use --help)"; exit 1 ;;
    esac
done

# --- Pre-flight checks ----------------------------------------------------
need() { command -v "$1" >/dev/null || { echo "missing: $1"; exit 1; }; }
need tmux
need git
need "$CLAUDE_BIN"

# --- Clean ----------------------------------------------------------------
clean_session() {
    tmux kill-session -t "$TMUX_SESSION" 2>/dev/null || true
    if [ -d "$WORKTREE_BASE" ]; then
        for wt in "$WORKTREE_BASE"/*; do
            [ -d "$wt" ] || continue
            git worktree remove --force "$wt" 2>/dev/null || rm -rf "$wt"
        done
    fi
}
if [ "$CLEAN_ONLY" -eq 1 ]; then
    clean_session
    echo "cleaned"
    exit 0
fi
clean_session

# --- Discover tasks -------------------------------------------------------
mapfile -t PROMPTS < <(ls "$PROMPT_DIR"/*.md | sort)
if [ "${#PROMPTS[@]}" -eq 0 ]; then
    echo "No prompts found in $PROMPT_DIR"; exit 1
fi

mkdir -p "$WORKTREE_BASE"

# --- Spin up worktrees + tmux windows -------------------------------------
echo "starting ${#PROMPTS[@]} parallel audit tasks"
echo "  worktrees: $WORKTREE_BASE"
echo "  session:   $TMUX_SESSION"
echo "  branch:    ${BRANCH_PREFIX}<task-id>"
echo "  base:      $BASE_REF"
echo

FIRST=1
for prompt in "${PROMPTS[@]}"; do
    name=$(basename "$prompt" .md)         # e.g. 03-sci-uart
    task_id=${name#[0-9][0-9]-}            # e.g. sci-uart
    branch="${BRANCH_PREFIX}${task_id}"
    worktree="$WORKTREE_BASE/$task_id"

    echo "[$task_id] worktree=$worktree branch=$branch"

    if [ "$DRY_RUN" -eq 1 ]; then
        continue
    fi

    git -C "$REPO_DIR" worktree add -b "$branch" "$worktree" "$BASE_REF"

    # Build the inner command. Cat the prompt file as the first message and
    # let Claude work autonomously. The shell prompt remains after Claude
    # exits so the human can inspect.
    inner=$(cat <<EOF
cd '$worktree' && \
echo '=== prompt ===' && cat '$prompt' && echo && \
echo '=== launching claude ===' && \
$CLAUDE_BIN $CLAUDE_FLAGS "\$(cat '$prompt')"; \
echo; echo '=== done ([$task_id]) -- press Enter to close ==='; read
EOF
)

    if [ "$FIRST" -eq 1 ]; then
        tmux new-session -d -s "$TMUX_SESSION" -n "$task_id" "$inner"
        FIRST=0
    else
        tmux new-window -t "$TMUX_SESSION" -n "$task_id" "$inner"
    fi
done

echo
echo "all tasks launched. attach with:"
echo "    tmux attach -t $TMUX_SESSION"
echo
echo "tear down with:"
echo "    bash scripts/hal-audit/launch.sh --clean"
