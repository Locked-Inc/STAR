#!/usr/bin/env bash
# Launch all 11 RX72N manual verification sessions in one tmux session.
# Each window verifies one chapter in its own git worktree.
#
# Prerequisites:
#   - tmux installed
#   - Worktrees created (the 11 git worktree add commands already run)
#
# Usage:
#   bash scripts/verify-parallel.sh
#
# Navigate windows: Ctrl-b n (next), Ctrl-b p (prev), Ctrl-b w (picker)

set -euo pipefail

SESSION="rx-verify"
BASE="/home/bsikar/Documents/github"
SCRIPTS_DIR="/tmp/rx-verify-runners"
mkdir -p "$SCRIPTS_DIR"

tmux kill-session -t "$SESSION" 2>/dev/null || true

# -- helpers ------------------------------------------------------------------

first_window=true
open_window() {
    local win="$1" dir="$2" script="$3"
    if $first_window; then
        tmux new-session -d -s "$SESSION" -n "$win" -c "$dir" -- bash "$script"
        first_window=false
    else
        tmux new-window -t "$SESSION" -n "$win" -c "$dir" -- bash "$script"
    fi
}

# -- Ch 3+11: LPC -------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch3-lpc.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch3-lpc
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch3-lpc, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 3 (Operating Modes) and Ch 11 (Low Power Consumption). Do not touch any other chapter.

Chapter details:
- Manual pages: 195-202 (Ch 3) and 401-449 (Ch 11) at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_lpc_regs.h
- Also check any usage in star-rx72n-firmware/libs/rx_hal/src/ for implementation values

Read every page in those ranges. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(lpc): <description>
2. VERIFICATION_LOG.md updated with Ch 3+11 findings
3. VERIFICATION_PLAN.md updated: mark Ch 3+11 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch3-lpc is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 3+11 LPC" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch3-lpc.sh"
open_window "ch3-lpc" "$BASE/STAR-ch3-lpc" "$SCRIPTS_DIR/ch3-lpc.sh"

# -- Ch 8: LVDA ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch8-lvda.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch8-lvda
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch8-lvda, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 8 (LVDA Voltage Detection). Do not touch any other chapter.

Chapter details:
- Manual pages: 316-334 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_lvda_regs.h
- Also check any usage in star-rx72n-firmware/libs/rx_hal/src/ for implementation values

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(lvda): <description>
2. VERIFICATION_LOG.md updated with Ch 8 findings
3. VERIFICATION_PLAN.md updated: mark Ch 8 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch8-lvda is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 8 LVDA" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch8-lvda.sh"
open_window "ch8-lvda" "$BASE/STAR-ch8-lvda" "$SCRIPTS_DIR/ch8-lvda.sh"

# -- Ch 9: Clock ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch9-clock.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch9-clock
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch9-clock, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 9 (Clock Generation). Do not touch any other chapter.

Chapter details:
- Manual pages: 335-389 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code files:
  - star-rx72n-firmware/libs/rx_hal/inc/rx72n_system_regs.h (SCKCR, PLL, oscillator regs)
  - star-rx72n-firmware/libs/rx_hal/inc/rx72n_clock.h (clock frequency constants)
- Also check star-rx72n-firmware/libs/rx_core/src/rx_infrastructure.c for clock init values

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(clock): <description>
2. VERIFICATION_LOG.md updated with Ch 9 findings
3. VERIFICATION_PLAN.md updated: mark Ch 9 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch9-clock is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 9 Clock" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch9-clock.sh"
open_window "ch9-clock" "$BASE/STAR-ch9-clock" "$SCRIPTS_DIR/ch9-clock.sh"

# -- Ch 21: ELC ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch21-elc.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch21-elc
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch21-elc, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 21 (ELC Event Link Controller). Do not touch any other chapter.

Chapter details:
- Manual pages: 835-860 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_elc_regs.h

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(elc): <description>
2. VERIFICATION_LOG.md updated with Ch 21 findings
3. VERIFICATION_PLAN.md updated: mark Ch 21 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch21-elc is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 21 ELC" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch21-elc.sh"
open_window "ch21-elc" "$BASE/STAR-ch21-elc" "$SCRIPTS_DIR/ch21-elc.sh"

# -- Ch 25+27: POE3 / POEG ----------------------------------------------------
cat > "$SCRIPTS_DIR/ch25-poe.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch25-poe
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch25-poe, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 25 (POE3a) and Ch 27 (POEG). Do not touch any other chapter.

Chapter details:
- Ch 25 POE3a manual pages: 1199-1235 at docs/rx72n-manual/pages/page-NNNN.pdf
  Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_poe3_regs.h
- Ch 27 POEG manual pages: 1432-1441 at docs/rx72n-manual/pages/page-NNNN.pdf
  Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_poeg_regs.h
- Also check: star-rx72n-firmware/libs/rx_hal/src/rx_poeg.c

Read every page in those ranges. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

Note: for POEG interrupt vector numbers (groups A-D), cross-check against Ch 15 ICU table (pages 466-541) to confirm the IRQ numbers are correct.

When complete:
1. All fixes committed using format: fix(poeg): <description> or fix(poe3): <description>
2. VERIFICATION_LOG.md updated with Ch 25+27 findings
3. VERIFICATION_PLAN.md updated: mark Ch 25+27 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch25-poe is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 25+27 POE" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch25-poe.sh"
open_window "ch25-poe" "$BASE/STAR-ch25-poe" "$SCRIPTS_DIR/ch25-poe.sh"

# -- Ch 28: TPU ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch28-tpu.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch28-tpu
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch28-tpu, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 28 (TPU Timer Pulse Unit). Do not touch any other chapter.

Chapter details:
- Manual pages: 1442-1519 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_tpu_regs.h
- Also check:
  - star-rx72n-firmware/libs/rx_hal/src/rx_tpu.c
  - star-rx72n-firmware/libs/rx_encoder/src/rx_encoder_tpu.c

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(tpu): <description>
2. VERIFICATION_LOG.md updated with Ch 28 findings
3. VERIFICATION_PLAN.md updated: mark Ch 28 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch28-tpu is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 28 TPU" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch28-tpu.sh"
open_window "ch28-tpu" "$BASE/STAR-ch28-tpu" "$SCRIPTS_DIR/ch28-tpu.sh"

# -- Ch 40: USB ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch40-usb.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch40-usb
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch40-usb, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 40 (USB 2.0 Full-Speed). Do not touch any other chapter.

Chapter details:
- Manual pages: 1928-2036 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_usb_regs.h
- Also check star-rx72n-firmware/libs/rx_usb/ for implementation values

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(usb): <description>
2. VERIFICATION_LOG.md updated with Ch 40 findings
3. VERIFICATION_PLAN.md updated: mark Ch 40 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch40-usb is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 40 USB" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch40-usb.sh"
open_window "ch40-usb" "$BASE/STAR-ch40-usb" "$SCRIPTS_DIR/ch40-usb.sh"

# -- Ch 41: SCI ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch41-sci.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch41-sci
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch41-sci, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 41 (SCI Serial Communications Interface). Do not touch any other chapter.

Chapter details:
- Manual pages: 2037-2216 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_sci_regs.h

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(sci): <description>
2. VERIFICATION_LOG.md updated with Ch 41 findings
3. VERIFICATION_PLAN.md updated: mark Ch 41 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch41-sci is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 41 SCI" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch41-sci.sh"
open_window "ch41-sci" "$BASE/STAR-ch41-sci" "$SCRIPTS_DIR/ch41-sci.sh"

# -- Ch 56: ADC ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch56-adc.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch56-adc
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch56-adc, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 56 (S12AD 12-bit ADC). Do not touch any other chapter.

Chapter details:
- Manual pages: 2809-2949 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_adc_regs.h
- Also check:
  - star-rx72n-firmware/libs/rx_hal/src/adc.c
  - star-rx72n-firmware/libs/rx_bus/src/rx_bus_adc.c

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(adc): <description>
2. VERIFICATION_LOG.md updated with Ch 56 findings
3. VERIFICATION_PLAN.md updated: mark Ch 56 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch56-adc is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 56 ADC" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch56-adc.sh"
open_window "ch56-adc" "$BASE/STAR-ch56-adc" "$SCRIPTS_DIR/ch56-adc.sh"

# -- Ch 60: RAM ---------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch60-ram.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch60-ram
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch60-ram, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 60 (RAM Control). Do not touch any other chapter.

Chapter details:
- Manual pages: 2977-2990 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_ram_regs.h

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(ram): <description>
2. VERIFICATION_LOG.md updated with Ch 60 findings
3. VERIFICATION_PLAN.md updated: mark Ch 60 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch60-ram is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 60 RAM" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch60-ram.sh"
open_window "ch60-ram" "$BASE/STAR-ch60-ram" "$SCRIPTS_DIR/ch60-ram.sh"

# -- Ch 62: Flash -------------------------------------------------------------
cat > "$SCRIPTS_DIR/ch62-flash.sh" << 'SCRIPT_EOF'
#!/usr/bin/env bash
cd /home/bsikar/Documents/github/STAR-ch62-flash
PROMPT=$(cat << 'PROMPT_EOF'
You are in a git worktree on branch feat/verify-ch62-flash, working on the STAR RX72N firmware.

Read the verification plan at docs/rx72n-manual/VERIFICATION_PLAN.md for all rules, context, and logging format. Follow every rule exactly.

Your scope is ONLY Ch 62 (Flash Memory). Do not touch any other chapter.

Chapter details:
- Manual pages: 2992-3233 at docs/rx72n-manual/pages/page-NNNN.pdf
- Code file: star-rx72n-firmware/libs/rx_hal/inc/rx72n_flash_regs.h

Read every page in that range. For each register, offset, bit mask, and constant in the manual, find the corresponding value in the code and verify it. Fix anything wrong. Log available-but-unused features in docs/rx72n-manual/AVAILABLE_FEATURES.md.

When complete:
1. All fixes committed using format: fix(flash): <description>
2. VERIFICATION_LOG.md updated with Ch 62 findings
3. VERIFICATION_PLAN.md updated: mark Ch 62 row as [x] COMPLETE
4. Inform the user that branch feat/verify-ch62-flash is ready to merge into feat/rx72n-manual-verification
PROMPT_EOF
)
exec /home/bsikar/.local/bin/claude --dangerously-skip-permissions --model sonnet --effort max --name "Ch 62 Flash" "$PROMPT"
SCRIPT_EOF
chmod +x "$SCRIPTS_DIR/ch62-flash.sh"
open_window "ch62-flash" "$BASE/STAR-ch62-flash" "$SCRIPTS_DIR/ch62-flash.sh"

# -- Attach --------------------------------------------------------------------
echo ""
echo "All 11 verification sessions launched in tmux session '$SESSION'."
echo ""
echo "  Attach:          tmux attach -t $SESSION"
echo "  Next window:     Ctrl-b n"
echo "  Prev window:     Ctrl-b p"
echo "  Window picker:   Ctrl-b w"
echo "  Detach:          Ctrl-b d"
echo ""
tmux attach -t "$SESSION"

# -- Merge all branches when done ---------------------------------------------
#
# After all 11 instances finish their work, run this from the STAR repo root
# to merge everything into feat/rx72n-manual-verification:
#
#   cd /home/bsikar/Documents/github/STAR
#   git checkout feat/rx72n-manual-verification
#   for branch in \
#       feat/verify-ch3-lpc \
#       feat/verify-ch8-lvda \
#       feat/verify-ch9-clock \
#       feat/verify-ch21-elc \
#       feat/verify-ch25-poe \
#       feat/verify-ch28-tpu \
#       feat/verify-ch40-usb \
#       feat/verify-ch41-sci \
#       feat/verify-ch56-adc \
#       feat/verify-ch60-ram \
#       feat/verify-ch62-flash; do
#       git merge "$branch" --no-edit || echo "CONFLICT on $branch -- resolve manually then continue"
#   done
#
# The only expected conflicts are in VERIFICATION_LOG.md and VERIFICATION_PLAN.md.
# Keep both sections when resolving (just concatenate the new entries).
