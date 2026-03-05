#!/usr/bin/env python3
"""check-file-path.py - Enforce first-line path comment policy for STAR project.

Every C/C++ source file must have its exact git-relative path as a block
comment on line 1:

    /* star-rx72n-firmware/libs/rx_hal/inc/rx72n_cmt_regs.h */

This makes every file self-identifying when read in isolation (editor tabs,
copy-paste, grep results) and allows tooling to verify the file has not
been moved without updating its header.

Usage:
    python3 scripts/utils/check-file-path.py              # scan all git-tracked files
    python3 scripts/utils/check-file-path.py file1 ...    # scan specific files
"""

import re
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration (must stay in sync with fix-copyright.py)
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

# Expected line-1 format: /* <path> */  (optional trailing whitespace)
PATH_COMMENT_RE = re.compile(r"^/\*\s*(.+?)\s*\*/\s*$")


# ---------------------------------------------------------------------------
# Per-file check
# ---------------------------------------------------------------------------

def check_file(path: Path, rel: str) -> list[str]:
    try:
        with path.open(encoding="utf-8", errors="replace") as fh:
            first_line = fh.readline().rstrip("\n")
    except OSError as exc:
        return [f"Cannot read file: {exc}"]

    m = PATH_COMMENT_RE.match(first_line)
    if not m:
        return [
            f"Line 1 is not a path comment\n"
            f"    found:    {first_line!r}\n"
            f"    expected: /* {rel} */"
        ]

    found_path = m.group(1)
    if found_path != rel:
        return [
            f"Line 1 path comment is wrong\n"
            f"    found:    /* {found_path} */\n"
            f"    expected: /* {rel} */"
        ]

    return []


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
    repo_root = Path(
        subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True, text=True,
        ).stdout.strip()
    )

    if len(sys.argv) > 1:
        candidates: list[tuple[Path, str]] = []
        for a in sys.argv[1:]:
            p = Path(a)
            try:
                rel = str(p.resolve().relative_to(repo_root))
            except ValueError:
                rel = str(p)
            if p.suffix in SOURCE_EXTENSIONS and not is_exempt(rel):
                candidates.append((p, rel))
    else:
        candidates = git_tracked_source_files(repo_root)

    failures: dict[str, list[str]] = {}
    for path, rel in candidates:
        errs = check_file(path, rel)
        if errs:
            failures[rel] = errs

    if not failures:
        print(f"[OK] Line-1 path comments correct in all {len(candidates)} checked file(s).")
        return 0

    print(f"[FAIL] Path comment violations found in {len(failures)} file(s):\n")
    for rel, errs in sorted(failures.items()):
        print(f"  {rel}")
        for e in errs:
            for line in e.splitlines():
                print(f"    {line}")
        print()

    print("Required format:  /* <git-relative-path> */  on line 1")
    return 1


if __name__ == "__main__":
    sys.exit(main())
