#!/usr/bin/env bash
#
# launch.sh -- spin up parallel Claude instances in tmux to audit the
#              RX72N HAL register definitions against ground-truth sources.
#
# Each task gets:
#   - a fresh git worktree under $WORKTREE_BASE
#   - a branch named $BRANCH_PREFIX<task-id>
#   - one tmux window running `claude` with that task's prompt
#   - per-prompt --model / --thinking / --effort flags read from a header
#     line in the prompt .md (see "Per-prompt flags" below)
#
# YOU MUST EXPLICITLY CHOOSE WHICH PROMPTS TO RUN. There is no "run all
# 17 prompts" default anymore -- a re-run accidentally re-burns budget on
# already-merged audit work, and forces a uniform model/effort profile
# across very different prompts. Use either --only or --all and decide.
#
# Requires: tmux, git, claude CLI (https://docs.anthropic.com/en/docs/claude-code)
#
# ----------------------------------------------------------------------
# Usage (from repo root):
#
#   bash scripts/hal-audit/launch.sh --only <pattern>[,<pattern>...]
#       Run prompts whose filename matches any of the comma-separated
#       substring patterns. Examples:
#           --only 17                        -> 17-community-cross-check.md
#           --only community                 -> 17-community-cross-check.md
#           --only mtu                       -> 05-mtu.md
#           --only sci-uart                  -> 03-sci-uart.md
#           --only 18,19,20,21,22            -> all 5 new-peripheral prompts
#           --only cac,doc,tmr,eccram,usb0   -> same five, by name
#       --only may also be passed multiple times; results are unioned.
#       If the result matches more than one prompt, you'll see them all
#       in the dry-run preview and be asked to confirm before launch.
#
#   bash scripts/hal-audit/launch.sh --preset <name>
#       Run a named preset. Defined presets:
#           new-stuff      -> 18-cac, 19-doc, 20-tmr, 21-eccram, 22-usb0,
#                             23-low-power (the new-peripheral
#                             implementation prompts added after the
#                             round-2 audit)
#           per-peripheral -> 01-system-clock through 16-misc-regs
#                             (the original audit pass)
#           cross-check    -> 17-community-cross-check
#           coverage-backfill
#                          -> 24..28: backfill rx_{cac,doc,tmr,eccram,lpc}
#                             unit-test coverage to 100/100/100 and drop
#                             their excludes from the CI coverage gate
#       --preset and --only may be combined; results are unioned.
#
#   bash scripts/hal-audit/launch.sh --all
#       Run every prompt in prompts/*.md in parallel. Re-burns budget on
#       audit work that's already in main. Asks for confirmation.
#
#   bash scripts/hal-audit/launch.sh --only <pattern> --dry-run
#   bash scripts/hal-audit/launch.sh --all              --dry-run
#       Print which prompts/branches/worktrees would be created. No edits.
#
#   bash scripts/hal-audit/launch.sh --clean --only <pattern>
#       Tear down ONLY the worktrees + branches for matching prompts (and
#       remove their tmux windows). Other in-flight audits are untouched.
#
#   bash scripts/hal-audit/launch.sh --clean --all
#       Tear down EVERY worktree, EVERY local + remote bsikar/verifying-*
#       branch, AND the tmux session. Aggressive. Use this when you're
#       sure no audit branch has unmerged work you want to keep.
#
# Then: tmux attach -t hal-audit
#       Ctrl-b w  -- window list (one per task)
#       Ctrl-b n  -- next window
#       Ctrl-b d  -- detach
#
# ----------------------------------------------------------------------
# Per-prompt flags
#
# Each prompt .md may declare its own claude CLI flags via a single line
# anywhere in the first 100 lines, in the form:
#
#   <!-- CLAUDE_FLAGS: --model claude-opus-4-7 --thinking enabled --effort medium --dangerously-skip-permissions -->
#
# If present, those flags override the script default (Opus 4.7 +
# thinking enabled + high effort). Different prompts can pick different
# models if needed; today they all share the default.
#
# If a prompt has no such header, the script default applies.
#
# Override the script default per-invocation via env:
#   CLAUDE_FLAGS='...' bash scripts/hal-audit/launch.sh --only <id>
#
# ----------------------------------------------------------------------

set -euo pipefail

REPO_DIR=$(git rev-parse --show-toplevel)
SCRIPT_DIR="$REPO_DIR/scripts/hal-audit"
PROMPT_DIR="$SCRIPT_DIR/prompts"
WORKTREE_BASE="${HAL_AUDIT_WORKTREE_BASE:-$HOME/star-hal-audit-worktrees}"
TMUX_SESSION="${HAL_AUDIT_SESSION:-hal-audit}"
BRANCH_PREFIX="${HAL_AUDIT_BRANCH_PREFIX:-bsikar/verifying-}"
BASE_REF="${HAL_AUDIT_BASE_REF:-main}"
CLAUDE_BIN="${CLAUDE_BIN:-claude}"

# Script default if a prompt has no <!-- CLAUDE_FLAGS: ... --> header.
# Opus 4.7 + thinking enabled + high effort is what we run for all the
# RX72N HAL audits now. The earlier Sonnet+max passes missed the kind of
# silently-wrong constants (PSEL=0x03 instead of 0x02, etc.) that we want
# to catch. Override per-invocation via env, or per-prompt via the header
# marker.
DEFAULT_CLAUDE_FLAGS="${CLAUDE_FLAGS:---model claude-opus-4-7 --thinking enabled --effort high --dangerously-skip-permissions}"

# --- Argument parsing -----------------------------------------------------

DRY_RUN=0
CLEAN=0
RUN_ALL=0
ONLY_PATTERNS=()       # accumulated from one or more --only invocations,
                       # each may be a comma-separated list of patterns.
PRESETS=()             # accumulated from one or more --preset invocations.

usage() { sed -n '3,70p' "$0"; }

# Resolve a preset name to a comma-separated list of patterns.
resolve_preset() {
    case "$1" in
        new-stuff)         echo "18,19,20,21,22,23" ;;
        per-peripheral)    echo "01,02,03,04,05,06,07,08,09,10,11,12,13,14,15,16" ;;
        cross-check)       echo "17" ;;
        coverage-backfill) echo "24,25,26,27,28" ;;
        *) echo "ERROR: unknown preset '$1' (known: new-stuff, per-peripheral, cross-check, coverage-backfill)" >&2; exit 2 ;;
    esac
}

while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run)   DRY_RUN=1 ;;
        --clean)     CLEAN=1 ;;
        --all)       RUN_ALL=1 ;;
        --only)      shift; ONLY_PATTERNS+=("${1:-}");;
        --only=*)    ONLY_PATTERNS+=("${1#--only=}");;
        --preset)    shift; PRESETS+=("${1:-}");;
        --preset=*)  PRESETS+=("${1#--preset=}");;
        --help|-h)   usage; exit 0 ;;
        *) echo "Unknown arg: $1 (use --help)"; exit 1 ;;
    esac
    shift
done

# Expand presets into the same pattern bag as --only.
for preset in "${PRESETS[@]}"; do
    ONLY_PATTERNS+=("$(resolve_preset "$preset")")
done

# Require an explicit selection. No more "bare invocation runs all 17."
if [ "$RUN_ALL" -eq 0 ] && [ "${#ONLY_PATTERNS[@]}" -eq 0 ]; then
    echo "ERROR: must specify --all, --only <pattern>, or --preset <name>." >&2
    echo "       Bare 'launch.sh' no longer fans out to every prompt." >&2
    echo "       Run 'launch.sh --help' for usage." >&2
    exit 2
fi

if [ "$RUN_ALL" -eq 1 ] && [ "${#ONLY_PATTERNS[@]}" -gt 0 ]; then
    echo "ERROR: --all is mutually exclusive with --only / --preset." >&2
    exit 2
fi

# --- Pre-flight checks ----------------------------------------------------
need() { command -v "$1" >/dev/null || { echo "missing: $1"; exit 1; }; }
need tmux
need git
need "$CLAUDE_BIN"

# --- Discover prompts (filtered) ------------------------------------------

mapfile -t ALL_PROMPTS < <(ls "$PROMPT_DIR"/*.md | sort)
if [ "${#ALL_PROMPTS[@]}" -eq 0 ]; then
    echo "No prompts found in $PROMPT_DIR" >&2; exit 1
fi

PROMPTS=()
if [ "$RUN_ALL" -eq 1 ]; then
    PROMPTS=("${ALL_PROMPTS[@]}")
else
    # Flatten ONLY_PATTERNS (which may have comma-separated entries)
    # into a single de-duplicated list of patterns.
    declare -A WANT_PATTERNS=()
    for entry in "${ONLY_PATTERNS[@]}"; do
        IFS=',' read -ra parts <<< "$entry"
        for part in "${parts[@]}"; do
            part="${part## }"; part="${part%% }"          # trim spaces
            [ -n "$part" ] && WANT_PATTERNS["$part"]=1
        done
    done
    # For each prompt, accept it if any pattern is a substring of its name.
    declare -A SEEN=()
    for p in "${ALL_PROMPTS[@]}"; do
        bn=$(basename "$p")
        for pat in "${!WANT_PATTERNS[@]}"; do
            if [[ "$bn" == *"$pat"* ]] && [ -z "${SEEN[$p]:-}" ]; then
                PROMPTS+=("$p")
                SEEN["$p"]=1
                break
            fi
        done
    done
fi

if [ "${#PROMPTS[@]}" -eq 0 ]; then
    echo "ERROR: no prompts matched the supplied --only / --preset patterns:" >&2
    for entry in "${ONLY_PATTERNS[@]}"; do echo "  '$entry'" >&2; done
    echo "Available prompts:" >&2
    for p in "${ALL_PROMPTS[@]}"; do echo "  $(basename "$p")" >&2; done
    exit 1
fi

# --- Helpers --------------------------------------------------------------

# extract per-prompt CLAUDE_FLAGS from a header marker like:
#   <!-- CLAUDE_FLAGS: --model claude-opus-4-7 --thinking enabled ... -->
# Falls back to $DEFAULT_CLAUDE_FLAGS if no marker found.
prompt_flags() {
    local prompt_file="$1"
    local extracted
    extracted=$(head -n 100 "$prompt_file" \
        | grep -oE '<!--\s*CLAUDE_FLAGS:[^-]*-->' \
        | head -n 1 \
        | sed -E 's|<!--\s*CLAUDE_FLAGS:\s*||; s|\s*-->||')
    if [ -n "$extracted" ]; then
        echo "$extracted"
    else
        echo "$DEFAULT_CLAUDE_FLAGS"
    fi
}

# Resolve a prompt path -> task_id / branch / worktree
prompt_meta() {
    local prompt_file="$1"
    local name task_id
    name=$(basename "$prompt_file" .md)         # e.g. 03-sci-uart
    task_id=${name#[0-9][0-9]-}                 # e.g. sci-uart
    echo "$task_id"
}

# --- Clean (targeted) -----------------------------------------------------

clean_one() {
    local task_id="$1"
    local branch="${BRANCH_PREFIX}${task_id}"
    local worktree="$WORKTREE_BASE/$task_id"

    # Kill that task's tmux window if the session exists
    if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
        tmux kill-window -t "$TMUX_SESSION:$task_id" 2>/dev/null || true
        # If that was the last window, the session is gone -- harmless.
    fi

    if [ -d "$worktree" ]; then
        git -C "$REPO_DIR" worktree remove --force "$worktree" 2>/dev/null \
            || rm -rf "$worktree"
    fi
    git -C "$REPO_DIR" worktree prune 2>/dev/null || true
    git -C "$REPO_DIR" branch -D "$branch" 2>/dev/null || true
    git -C "$REPO_DIR" push origin --delete "$branch" 2>/dev/null || true
    echo "  cleaned $task_id (branch + worktree + remote + tmux window)"
}

clean_all() {
    tmux kill-session -t "$TMUX_SESSION" 2>/dev/null || true

    if [ -d "$WORKTREE_BASE" ]; then
        for wt in "$WORKTREE_BASE"/*; do
            [ -d "$wt" ] || continue
            git -C "$REPO_DIR" worktree remove --force "$wt" 2>/dev/null \
                || rm -rf "$wt"
        done
    fi
    git -C "$REPO_DIR" worktree prune 2>/dev/null || true

    for b in $(git -C "$REPO_DIR" for-each-ref --format='%(refname:short)' \
               "refs/heads/${BRANCH_PREFIX}*" 2>/dev/null); do
        git -C "$REPO_DIR" branch -D "$b" 2>/dev/null || true
    done

    REMOTE_BRANCHES=$(git -C "$REPO_DIR" ls-remote --heads origin "${BRANCH_PREFIX}*" 2>/dev/null \
                      | awk '{print $2}' | sed 's|refs/heads/||')
    for b in $REMOTE_BRANCHES; do
        echo "  deleting remote $b"
        git -C "$REPO_DIR" push origin --delete "$b" 2>/dev/null || true
    done
    echo "  cleaned all bsikar/verifying-* (worktrees + local + remote + tmux)"
}

if [ "$CLEAN" -eq 1 ]; then
    if [ "$RUN_ALL" -eq 1 ]; then
        clean_all
    else
        for p in "${PROMPTS[@]}"; do clean_one "$(prompt_meta "$p")"; done
    fi
    exit 0
fi

# --- Pre-launch summary + safety prompt -----------------------------------

echo "==> Will launch ${#PROMPTS[@]} task(s):"
for p in "${PROMPTS[@]}"; do
    task_id=$(prompt_meta "$p")
    flags=$(prompt_flags "$p")
    printf "  %-30s  %s\n" "$task_id" "$flags"
done
echo "  worktrees: $WORKTREE_BASE/<task_id>"
echo "  session:   $TMUX_SESSION"
echo "  branches:  ${BRANCH_PREFIX}<task_id>  (off $BASE_REF)"
echo

if [ "$DRY_RUN" -eq 1 ]; then
    echo "(dry-run; nothing launched)"
    exit 0
fi

# Confirm before launching more than 1 task at a time. Single-task runs
# proceed without prompting -- common case during iterative work.
if [ "${#PROMPTS[@]}" -gt 1 ]; then
    read -r -p "Launch all ${#PROMPTS[@]} in parallel? [y/N] " ans
    case "$ans" in
        y|Y|yes|YES) ;;
        *) echo "aborted"; exit 1 ;;
    esac
fi

mkdir -p "$WORKTREE_BASE"

# --- Spin up worktrees + tmux windows -------------------------------------

FIRST=1
# Reuse an existing session if it's already up; otherwise we'll create it
# on the first task.
if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    FIRST=0
fi

for prompt in "${PROMPTS[@]}"; do
    task_id=$(prompt_meta "$prompt")
    branch="${BRANCH_PREFIX}${task_id}"
    worktree="$WORKTREE_BASE/$task_id"
    flags=$(prompt_flags "$prompt")

    # If a stale worktree/branch already exists, refuse rather than
    # silently nuking it. Force the user to --clean --only <task> first.
    if [ -d "$worktree" ] || \
       git -C "$REPO_DIR" rev-parse --verify "$branch" >/dev/null 2>&1; then
        echo "ERROR: existing worktree/branch for '$task_id'." >&2
        echo "       Run: bash scripts/hal-audit/launch.sh --clean --only $task_id" >&2
        exit 1
    fi

    echo "[$task_id] worktree=$worktree branch=$branch"
    git -C "$REPO_DIR" worktree add -b "$branch" "$worktree" "$BASE_REF"

    inner=$(cat <<EOF
cd '$worktree' && \
echo '=== prompt ===' && cat '$prompt' && echo && \
echo '=== flags ===' && echo '$flags' && echo && \
echo '=== launching claude ===' && \
$CLAUDE_BIN $flags "\$(cat '$prompt')"; \
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
echo "launched. attach with:"
echo "    tmux attach -t $TMUX_SESSION"
echo
echo "to tear down what you just launched:"
if [ "$RUN_ALL" -eq 1 ]; then
    echo "    bash scripts/hal-audit/launch.sh --clean --all"
else
    # Print a clean --clean line that matches what was launched.
    teardown_args=""
    for p in "${PROMPTS[@]}"; do
        task_id=$(prompt_meta "$p")
        teardown_args+=" --only $task_id"
    done
    echo "    bash scripts/hal-audit/launch.sh --clean$teardown_args"
fi
