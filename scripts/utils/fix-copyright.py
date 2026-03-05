#!/usr/bin/env python3
"""fix-copyright.py - Fix copyright headers across all STAR C/C++ source files.

Desired file structure after running this script:

    /* star-rx72n-firmware/libs/rx_hal/inc/rx72n_cmt_regs.h */   <- line 1, exact git-relative path
    /**
     * @file rx72n_cmt_regs.h
     * ...
     * @copyright Copyright (c) 2026 Locked Inc.
     * SPDX-License-Identifier: MIT                              <- inside Doxygen, after @copyright
     * @since ...
     */

For Renesas-derived files (detected by Renesas copyright text):
    @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
    (no SPDX -- Renesas has its own proprietary licence block)

Usage:
    python3 scripts/utils/fix-copyright.py              # fix all git-tracked source files
    python3 scripts/utils/fix-copyright.py file1 ...   # fix specific files
    python3 scripts/utils/fix-copyright.py --dry-run   # show what would change, do not write
"""

import re
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration (must stay in sync with check-copyright.py / check-file-path.py)
# ---------------------------------------------------------------------------

SOURCE_EXTENSIONS = {".c", ".h", ".cpp", ".hpp"}

EXEMPT_PREFIXES = (
    "schematic/",
    "star-rx72n-firmware/libs/rx_nanopb/nanopb/",
    "star-rx72n-firmware/libs/rx_nanopb/inc/gen/",
    "star-rx72n-firmware/libs/threadx/",
    "star-ros2/src/sllidar_ros2/",
    "star-ros2/install/",
    "star-rx72n-firmware/build/",
)

RENESAS_MARKER_RE = re.compile(
    r"Copyright.*Renesas Electronics Corporation",
    re.IGNORECASE,
)

OUR_COPYRIGHT     = "Copyright (c) 2026 Locked Inc."
RENESAS_COPYRIGHT = ("Copyright (c) 2026 Locked Inc."
                     " Based on Renesas Electronics Corporation source.")

# Matches any @copyright tag line in a Doxygen block
COPYRIGHT_TAG_RE = re.compile(r"^([^\S\n]*\*[^\S\n]*)@copyright\s+.*$", re.MULTILINE)

# Matches a standalone SPDX line (not inside a Doxygen * block)
# Covers both: // SPDX... and /* SPDX... */
STANDALONE_SPDX_RE = re.compile(
    r"^(?![ \t]*\*)[ \t]*(?://|/\*)[ \t]*SPDX-License-Identifier:[ \t]*MIT[ \t]*(?:\*/)?[ \t]*\n",
    re.MULTILINE,
)

# Path comment on line 1: /* some/path/file.ext */
PATH_COMMENT_RE = re.compile(r"^/\*[^*\n]+\*/[ \t]*\n")

# Also matches // style header lines used by some ROS2 files
SLASH_COMMENT_LINE_RE = re.compile(r"^//[^\n]*\n")

# Detect whether file has a file-level Doxygen block (contains @file tag)
FILE_LEVEL_DOXYGEN_RE = re.compile(r"/\*\*[^*].*?@file\b", re.DOTALL)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def doxygen_line_prefix(block_text: str) -> str:
    """Infer the ' * ' prefix style used in a Doxygen block."""
    for line in block_text.splitlines():
        stripped = line.lstrip()
        if stripped.startswith("* ") or stripped == "*":
            indent = line[: len(line) - len(stripped)]
            return indent + "* "
    return " * "


def insert_spdx_after_copyright(text: str) -> str:
    """Insert 'SPDX-License-Identifier: MIT' on the line immediately after @copyright."""
    def replacer(m: re.Match) -> str:
        prefix = m.group(1)   # e.g. ' * '
        return m.group(0).rstrip("\n") + "\n" + prefix + "SPDX-License-Identifier: MIT"
    return COPYRIGHT_TAG_RE.sub(replacer, text, count=1)


def find_file_doxygen_block(text: str) -> tuple[int, int] | None:
    """Return (start, end) of the file-level /** block (the one with @file).
    Returns None if no such block exists."""
    pos = 0
    while True:
        start = text.find("/**", pos)
        if start == -1:
            return None
        end = text.find("*/", start + 3)
        if end == -1:
            return None
        block = text[start:end]
        if "@file" in block:
            return start, end
        pos = end + 2


def fix_or_insert_copyright(text: str, expected: str) -> str:
    """Replace an existing @copyright value, or add one to the file-level Doxygen block."""
    if COPYRIGHT_TAG_RE.search(text):
        def replacer(m: re.Match) -> str:
            prefix = m.group(1)
            return f"{prefix}@copyright {expected}"
        return COPYRIGHT_TAG_RE.sub(replacer, text, count=1)

    # No @copyright found -- insert before closing */ of the file-level /** block
    span = find_file_doxygen_block(text)
    if span is None:
        return text   # handled separately for no-Doxygen files
    start, end = span

    block = text[start:end]
    prefix = doxygen_line_prefix(block)
    insert = f"\n{prefix}@copyright {expected}"
    return text[:end] + insert + "\n" + text[end:]


def fix_path_comment(text: str, rel: str) -> str:
    """Ensure line 1 is exactly '/* rel */' followed by a blank line."""
    desired = f"/* {rel} */\n"

    # Strip existing line-1 path comment or // comment block preamble
    # We remove lines that look like path comments or SPDX / old // headers
    lines = text.splitlines(keepends=True)
    strip_count = 0
    for line in lines:
        if (PATH_COMMENT_RE.match(line)
                or STANDALONE_SPDX_RE.match(line)
                or (strip_count == 0 and SLASH_COMMENT_LINE_RE.match(line)
                    and "SPDX" in line)):
            strip_count += 1
        else:
            break

    # Only strip the very first line if it looks like an old path comment or SPDX
    # (avoid ripping out real // file headers for non-Doxygen files)
    first = lines[0] if lines else ""
    if PATH_COMMENT_RE.match(first) or (STANDALONE_SPDX_RE.match(first) and "SPDX" in first):
        rest = "".join(lines[1:])
    else:
        rest = text

    # Ensure exactly one blank line between the path comment and the body
    return desired + "\n" + rest.lstrip("\n")


def build_minimal_doxygen(rel: str, filename: str, existing_lines: list[str]) -> str:
    """Build a minimal /** ... */ block for files that have no Doxygen header."""
    # Try to derive a brief from the first real // comment line
    brief = ""
    for line in existing_lines:
        stripped = line.strip()
        if stripped.startswith("//"):
            candidate = stripped.lstrip("/ ").strip()
            # Skip lines that are just the filename or SPDX/copyright noise
            if (candidate
                    and not candidate.startswith("SPDX")
                    and not candidate.startswith("Copyright")
                    and not candidate.lower().startswith("@copyright")
                    and filename not in candidate[:30]):
                brief = candidate
                break
    if not brief:
        brief = f"{filename} - STAR project source file"

    return (
        f"/**\n"
        f" * @file {filename}\n"
        f" * @brief {brief}\n"
        f" * @copyright Copyright (c) 2026 Locked Inc.\n"
        f" * SPDX-License-Identifier: MIT\n"
        f" */\n"
    )


def strip_noise_header(text: str) -> str:
    """Remove standalone SPDX, bare // Copyright, and stray // @copyright lines."""
    # Remove standalone SPDX lines anywhere (will be re-added inside Doxygen)
    text = STANDALONE_SPDX_RE.sub("", text)
    # Remove stray // @copyright lines added by previous fix passes
    text = re.sub(r"^[ \t]*//[ \t]*@copyright[^\n]*\n", "", text, flags=re.MULTILINE)
    # Remove bare // Copyright lines (no @, common in ROS2 files) and any
    # following // licence boilerplate block
    text = re.sub(
        r"^[ \t]*//[ \t]*Copyright[^\n]*\n"
        r"(?:^[ \t]*//[^\n]*\n)*",
        "", text, flags=re.MULTILINE
    )
    return text


# ---------------------------------------------------------------------------
# Per-file entry point
# ---------------------------------------------------------------------------

def fix_file(path: Path, rel: str, dry_run: bool) -> bool:
    try:
        original = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"  ERROR reading {path}: {exc}")
        return False

    text    = original
    is_renesas = bool(RENESAS_MARKER_RE.search(text))
    expected   = RENESAS_COPYRIGHT if is_renesas else OUR_COPYRIGHT
    filename   = path.name

    if FILE_LEVEL_DOXYGEN_RE.search(text):
        # --- Files with a file-level /** Doxygen block (has @file tag) ---
        # 1. Remove standalone SPDX from anywhere outside Doxygen
        text = STANDALONE_SPDX_RE.sub("", text)
        # 2. Fix the @copyright value (or insert it)
        text = fix_or_insert_copyright(text, expected)
        # 3. Insert SPDX right after @copyright (non-Renesas only)
        if not is_renesas:
            # Only insert if not already inside Doxygen
            if "SPDX-License-Identifier: MIT" not in text:
                text = insert_spdx_after_copyright(text)
        # 4. Fix line-1 path comment
        text = fix_path_comment(text, rel)

    else:
        # --- Files without a /** Doxygen block (some ROS2 files) ---
        # Strip all the noisy header lines, then prepend a clean Doxygen block
        existing_lines = text.splitlines(keepends=True)
        text = strip_noise_header(text)

        if not is_renesas:
            doxy = build_minimal_doxygen(rel, filename, existing_lines)
            # Remove the first line if it's already an old path comment / SPDX
            lines = text.splitlines(keepends=True)
            if lines and (PATH_COMMENT_RE.match(lines[0]) or "SPDX" in lines[0]):
                text = "".join(lines[1:])
            text = f"/* {rel} */\n\n" + doxy + text
        else:
            text = fix_path_comment(text, rel)
            text = fix_or_insert_copyright(text, expected)

    if text == original:
        return False

    if not dry_run:
        path.write_text(text, encoding="utf-8")
    return True


# ---------------------------------------------------------------------------
# File discovery
# ---------------------------------------------------------------------------

def is_exempt(rel: str) -> bool:
    return any(rel.startswith(p) for p in EXEMPT_PREFIXES)


def git_tracked_source_files(repo_root: Path) -> list[tuple[Path, str]]:
    result = subprocess.run(
        ["git", "ls-files", "--", "*.c", "*.h", "*.cpp", "*.hpp"],
        capture_output=True, text=True, cwd=repo_root,
    )
    out = []
    for line in result.stdout.splitlines():
        rel = line.strip()
        if rel and not is_exempt(rel):
            out.append((repo_root / rel, rel))
    return out


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    args    = sys.argv[1:]
    dry_run = "--dry-run" in args
    args    = [a for a in args if a != "--dry-run"]

    repo_root = Path(
        subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True, text=True,
        ).stdout.strip()
    )

    if args:
        candidates: list[tuple[Path, str]] = []
        for a in args:
            p = Path(a)
            try:
                rel = str(p.resolve().relative_to(repo_root))
            except ValueError:
                rel = str(p)
            if p.suffix in SOURCE_EXTENSIONS and not is_exempt(rel):
                candidates.append((p, rel))
    else:
        candidates = git_tracked_source_files(repo_root)

    changed = 0
    for path, rel in candidates:
        if fix_file(path, rel, dry_run):
            action = "would fix" if dry_run else "fixed"
            print(f"  {action}: {rel}")
            changed += 1

    verb = "would be changed" if dry_run else "changed"
    print(f"\n[{'DRY RUN' if dry_run else 'DONE'}] {changed} file(s) {verb}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
