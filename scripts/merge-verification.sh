#!/usr/bin/env bash
# Launch a single Claude (Opus, max effort) to merge all 11 verification
# branches into feat/rx72n-manual-verification and resolve conflicts.
#
# Usage:
#   bash scripts/merge-verification.sh

set -euo pipefail

SESSION="rx-merge"
REPO="/home/bsikar/Documents/github/STAR"
SCRIPT="/tmp/rx-merge-runner.sh"

tmux kill-session -t "$SESSION" 2>/dev/null || true

cat > "$SCRIPT" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR

PROMPT=$(cat << 'PROMPT_EOF'
You are in the main STAR repository at /home/bsikar/Documents/github/STAR.

Your job is to merge all 11 completed RX72N manual verification branches into
feat/rx72n-manual-verification. Each branch was worked on in isolation and has
commits for one chapter. The branches are:

  feat/verify-ch3-lpc
  feat/verify-ch8-lvda
  feat/verify-ch9-clock
  feat/verify-ch21-elc
  feat/verify-ch25-poe
  feat/verify-ch28-tpu
  feat/verify-ch40-usb
  feat/verify-ch41-sci
  feat/verify-ch56-adc
  feat/verify-ch60-ram
  feat/verify-ch62-flash

Steps:
1. Check out feat/rx72n-manual-verification.
2. For each branch above (in order), run: git merge <branch> --no-edit
3. If a merge conflict occurs, it will be in VERIFICATION_LOG.md or
   VERIFICATION_PLAN.md (the only shared files). Resolve it by keeping ALL
   content from both sides -- do not discard any chapter's log entries or
   checklist updates. Then: git add <file> && git merge --continue
4. After all 11 branches are merged, verify VERIFICATION_PLAN.md has all
   11 chapters marked [x] COMPLETE and VERIFICATION_LOG.md has entries for
   all 11 chapters.
5. If any chapter is missing or incomplete, fix it before finishing.
6. Delete all 11 feat/verify-ch* branches (they are no longer needed).
7. Remove all 11 git worktrees:
     git worktree remove ../STAR-ch3-lpc  --force
     git worktree remove ../STAR-ch8-lvda  --force
     git worktree remove ../STAR-ch9-clock --force
     git worktree remove ../STAR-ch21-elc  --force
     git worktree remove ../STAR-ch25-poe  --force
     git worktree remove ../STAR-ch28-tpu  --force
     git worktree remove ../STAR-ch40-usb  --force
     git worktree remove ../STAR-ch41-sci  --force
     git worktree remove ../STAR-ch56-adc  --force
     git worktree remove ../STAR-ch60-ram  --force
     git worktree remove ../STAR-ch62-flash --force
8. Report a summary: which chapters had fixes, which were clean, any issues.

Do not push to remote. Do not skip any branch.
PROMPT_EOF
)

exec /home/bsikar/.local/bin/claude \
    --dangerously-skip-permissions \
    --model opus \
    --effort max \
    --name "Verification Merge" \
    "$PROMPT"
SCRIPT_EOF

chmod +x "$SCRIPT"

tmux new-session -d -s "$SESSION" -n "merge" -c "$REPO" -- bash "$SCRIPT"

echo ""
echo "Merge session launched in tmux session '$SESSION'."
echo ""
echo "  Attach:  tmux attach -t $SESSION"
echo "  Detach:  Ctrl-b d"
echo ""
tmux attach -t "$SESSION"
