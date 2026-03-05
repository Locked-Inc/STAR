#!/usr/bin/env python3
"""fix-copyright.py - Fix copyright headers across all STAR C/C++ source files.

Applies the canonical copyright policy:

  Locked Inc. files:
      SPDX-License-Identifier: MIT   (inserted as line 1 if absent)
      @copyright Copyright (c) 2026 Locked Inc.

  Renesas-derived files (detected by Renesas copyright text in file):
      @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
      (no SPDX -- Renesas has its own proprietary licence block)

Usage:
    python3 scripts/utils/fix-copyright.py              # fix all git-tracked source files
    python3 scripts/utils/fix-copyright.py file1 file2  # fix specific files
    python3 scripts/utils/fix-copyright.py --dry-run    # show what would change, do not write
"""

import re
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration (must stay in sync with check-copyright.py)
# ---------------------------------------------------------------------------

SOURCE_EXTENSIONS = {".c", ".h", ".cpp", ".hpp"}

EXEMPT_PREFIXES = (
    "schematic/",
    "star-rx72n-firmware/libs/rx_nanopb/nanopb/",
    "star-rx72n-firmware/libs/threadx/",
    "star-ros2/src/sllidar_ros2/",
    "star-ros2/install/",
    "star-rx72n-firmware/build/",
)

RENESAS_MARKER_RE = re.compile(
    r"Copyright.*Renesas Electronics Corporation",
    re.IGNORECASE,
)

OUR_COPYRIGHT      = "Copyright (c) 2026 Locked Inc."
RENESAS_COPYRIGHT  = ("Copyright (c) 2026 Locked Inc."
                      " Based on Renesas Electronics Corporation source.")
SPDX_LINE          = "// SPDX-License-Identifier: MIT"

# Matches any existing @copyright tag line (captures leading whitespace + asterisk)
COPYRIGHT_LINE_RE = re.compile(r"^(\s*\*?\s*)@copyright\s+.*$", re.MULTILINE)

SPDX_PRESENT_RE = re.compile(r"SPDX-License-Identifier:\s*MIT", re.MULTILINE)  # matches // or /* */ form

# ---------------------------------------------------------------------------
# Fix helpers
# ---------------------------------------------------------------------------

def fix_copyright_tag(text: str, expected: str) -> tuple[str, bool]:
    """Replace or insert the @copyright tag. Returns (new_text, changed)."""
    def replacer(m):
        prefix = m.group(1)  # leading whitespace / " * "
        return f"{prefix}@copyright {expected}"

    if COPYRIGHT_LINE_RE.search(text):
        new_text = COPYRIGHT_LINE_RE.sub(replacer, text, count=1)
        return new_text, new_text != text

    # No @copyright tag at all -- insert before closing */ of first /** block.
    new_text = insert_copyright_into_doxygen(text, expected)
    if new_text is None:
        # No Doxygen block found; prepend a // comment after any SPDX line.
        new_text = prepend_copyright_comment(text, expected)
    return new_text, new_text != text


def insert_copyright_into_doxygen(text: str, expected: str) -> str | None:
    """Insert @copyright before the closing */ of the first /** block.
    Returns None if no /** block is found."""
    # Find the first /** opening
    start = text.find("/**")
    if start == -1:
        return None
    # Find its closing */
    end = text.find("*/", start + 3)
    if end == -1:
        return None

    # Walk back from end to find the last line in the block so we know indentation
    block = text[start:end]
    lines = block.splitlines()
    # Determine indentation from existing block lines (look for " * " pattern)
    prefix = " * "
    for line in lines:
        stripped = line.lstrip()
        if stripped.startswith("* ") or stripped == "*":
            prefix = line[: len(line) - len(line.lstrip())] + "* "
            break

    insert = f"\n{prefix}@copyright {expected}"
    return text[:end] + insert + "\n" + text[end:]


def prepend_copyright_comment(text: str, expected: str) -> str:
    """For files with no Doxygen block, prepend a // copyright line."""
    # Insert after SPDX line if present, else after first // comment block
    lines = text.splitlines(keepends=True)
    insert_at = 0
    for i, line in enumerate(lines):
        if line.startswith("// SPDX-License-Identifier:"):
            insert_at = i + 1
            break
        if line.startswith("//") or line.strip() == "":
            insert_at = i + 1
        else:
            break
    lines.insert(insert_at, f"// @copyright {expected}\n")
    return "".join(lines)


def fix_spdx(text: str) -> tuple[str, bool]:
    """Prepend SPDX line as line 1 if not already present."""
    if SPDX_PRESENT_RE.search(text):
        return text, False
    return SPDX_LINE + "\n" + text, True


# ---------------------------------------------------------------------------
# Per-file entry point
# ---------------------------------------------------------------------------

def fix_file(path: Path, dry_run: bool) -> bool:
    """Apply copyright fixes to *path*. Returns True if file was changed."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"  ERROR reading {path}: {exc}")
        return False

    is_renesas = bool(RENESAS_MARKER_RE.search(text))
    expected   = RENESAS_COPYRIGHT if is_renesas else OUR_COPYRIGHT
    changed    = False

    # 1. Fix @copyright tag
    text, c = fix_copyright_tag(text, expected)
    changed = changed or c

    # 2. Add SPDX (non-Renesas only)
    if not is_renesas:
        text, c = fix_spdx(text)
        changed = changed or c

    if changed:
        if not dry_run:
            path.write_text(text, encoding="utf-8")
        return True
    return False


# ---------------------------------------------------------------------------
# File discovery
# ---------------------------------------------------------------------------

def is_exempt(rel: str) -> bool:
    return any(rel.startswith(p) for p in EXEMPT_PREFIXES)


def git_tracked_source_files(repo_root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "--", "*.c", "*.h", "*.cpp", "*.hpp"],
        capture_output=True, text=True, cwd=repo_root,
    )
    paths = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if line and not is_exempt(line):
            paths.append(repo_root / line)
    return paths


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    args = sys.argv[1:]
    dry_run = "--dry-run" in args
    args = [a for a in args if a != "--dry-run"]

    repo_root = Path(
        subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True, text=True,
        ).stdout.strip()
    )

    if args:
        candidates = []
        for a in args:
            p = Path(a)
            try:
                rel = str(p.resolve().relative_to(repo_root))
            except ValueError:
                rel = str(p)
            if p.suffix in SOURCE_EXTENSIONS and not is_exempt(rel):
                candidates.append(p)
    else:
        candidates = git_tracked_source_files(repo_root)

    changed_count = 0
    for path in candidates:
        if fix_file(path, dry_run):
            try:
                rel = str(path.resolve().relative_to(repo_root))
            except ValueError:
                rel = str(path)
            action = "would fix" if dry_run else "fixed"
            print(f"  {action}: {rel}")
            changed_count += 1

    verb = "would be changed" if dry_run else "changed"
    print(f"\n[{'DRY RUN' if dry_run else 'DONE'}] {changed_count} file(s) {verb}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
