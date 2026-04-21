# RX72N HAL register audit -- parallel verification harness

Spins up N parallel Claude Sonnet instances in tmux, each one auditing one
peripheral's HAL register definitions against the authoritative RX72N HW
manual PDF.

## Why

Multiple HAL constants have been found wrong against the manual (PSEL=0x14
should be 0x1E for GPTW; SCI9 module-stop is `MSTPCRC.bit26` not
`MSTPCRB.bit22`; etc.). The HAL was clearly written from memory rather than
per-bit verification, so we're systematically re-checking everything before
the production firmware lands.

## How it works

```
scripts/hal-audit/
├── launch.sh                # tmux orchestrator (run this)
├── README.md                # you are here
└── prompts/
    ├── 01-system-clock.md   # one tight prompt per peripheral group
    ├── 02-mpc.md
    ├── 03-sci-uart.md
    ├── 04-gptw.md
    ├── 05-mtu.md
    ├── 06-tpu.md
    ├── 07-riic.md
    ├── 08-rspi.md
    ├── 09-adc.md
    ├── 10-port-gpio.md
    ├── 11-cmt.md
    ├── 12-poeg.md
    ├── 13-iwdt-wdt.md
    ├── 14-icu.md
    ├── 15-usb.md
    └── 16-misc-regs.md
```

Each prompt:
- Targets one specific peripheral / file group
- Names the manual chapter to use
- Tells the agent what to verify (base address, MSTP bit, register
  offsets, bit field positions, PSEL values)
- Tells it to commit and push to `bsikar/verifying-<peripheral>` once
  the firmware still builds
- Caps wall time at 30 minutes
- Stays under ~400 words to keep Sonnet's context tight

## Running

Prereqs:
- `tmux`
- `git` worktree support
- `claude` CLI installed (Anthropic's Claude Code)
- Repo cloned at `~/STAR` (or set `HAL_AUDIT_WORKTREE_BASE`)

Launch:
```bash
cd ~/STAR
bash scripts/hal-audit/launch.sh
tmux attach -t hal-audit
```

The session has one window per task. Switch with `Ctrl-b w` (window
list) or `Ctrl-b 0..9` (jump). Detach with `Ctrl-b d`.

Each task creates a fresh worktree at `$HAL_AUDIT_WORKTREE_BASE/<task>`
on a new branch `bsikar/verifying-<task>`, branched from `main`.

Tear down:
```bash
bash scripts/hal-audit/launch.sh --clean
```

Dry run (show what would happen, no actual launch):
```bash
bash scripts/hal-audit/launch.sh --dry-run
```

## Customizing

Override via env vars:
- `HAL_AUDIT_WORKTREE_BASE=/path` -- where worktrees live
- `HAL_AUDIT_SESSION=name` -- tmux session name
- `HAL_AUDIT_BRANCH_PREFIX=foo/bar-` -- branch naming
- `HAL_AUDIT_BASE_REF=develop` -- base branch (default `main`)
- `CLAUDE_BIN=/usr/local/bin/claude` -- claude binary
- `CLAUDE_FLAGS='--model claude-sonnet-4-6 --dangerously-skip-permissions'`

## After the audits finish

1. Review each branch: `git log bsikar/verifying-<task>`
2. Look for OK/FIX output in each tmux window's scrollback
3. Branches with overlapping edits (e.g. `02-mpc` and `04-gptw` both
   touching `rx_mpc.h`) will conflict on merge -- run a separate Opus
   review pass to resolve those.
4. Cherry-pick or merge clean branches into a single `bsikar/hal-audit`
   integration branch and PR from there.

## Adding a new peripheral

1. Drop a new prompt at `prompts/NN-<peripheral>.md` following the same
   shape as existing ones (cite manual chapter, list files, define
   acceptance criteria, name the branch).
2. Re-run `launch.sh` -- it auto-discovers prompts.
