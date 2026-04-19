#!/usr/bin/env python3
"""check-copyright.py - Enforce copyright header policy for STAR project.

Verifies that every C/C++ source file carries the correct copyright notice
and (for Locked Inc. owned files) an SPDX licence identifier.

Required for Locked Inc. owned files:
    SPDX-License-Identifier: MIT
    @copyright Copyright (c) 2026 Locked Inc.

Required for Renesas-derived files (detected by Renesas copyright text):
    @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
    (no SPDX required -- Renesas has its own proprietary licence block in the file)

Usage:
    python3 scripts/check-copyright.py              # scan all git-tracked source files
    python3 scripts/check-copyright.py file1 file2  # scan specific files
"""

import re
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SOURCE_EXTENSIONS = {".c", ".h", ".cpp", ".hpp"}

# Path prefixes (relative to repo root) that are third-party or generated
# code -- never checked.
EXEMPT_PREFIXES = (
    "schematic/",
    "gen/",  # top-level protobuf/nanopb generated code (buf output)
    "star-rx72n-firmware/libs/rx_nanopb/nanopb/",
    "star-rx72n-firmware/libs/rx_nanopb/inc/gen/",
    "star-rx72n-firmware/libs/threadx/",
    "star-ros2/src/sllidar_ros2/",
    "star-ros2/install/",
    "star-rx72n-firmware/build/",
    # Reference-only / experimental firmware variants not part of the
    # production build. pwm_test_fit/ is a Renesas-FIT-derived reference
    # variant retained for comparison against the HAL path.
    "star-rx72n-firmware/pwm_test_fit/",
)

# Identifies a Renesas-derived file by the presence of their copyright line.
# Matches the actual Renesas copyright/disclaimer, NOT casual mentions of the
# company name in comments (e.g. USB vendor IDs, string descriptors).
RENESAS_MARKER_RE = re.compile(
    r"Copyright.*Renesas Electronics Corporation",
    re.IGNORECASE,
)

# Canonical patterns -- must match exactly (no trailing license text, etc.)
OUR_COPYRIGHT_RE = re.compile(
    r"@copyright\s+Copyright \(c\) 2026 Locked Inc\.\s*$",
    re.MULTILINE,
)
RENESAS_COPYRIGHT_RE = re.compile(
    r"@copyright\s+Copyright \(c\) 2026 Locked Inc\. Based on Renesas Electronics Corporation source\.\s*$",
    re.MULTILINE,
)
SPDX_RE = re.compile(
    r"SPDX-License-Identifier:\s*MIT",
    re.MULTILINE,
)

# Catches any @copyright tag so we can report what was actually found.
ANY_COPYRIGHT_RE = re.compile(r"@copyright\s+(.+)", re.MULTILINE)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def find_existing_copyright(text: str) -> str:
    """Return the first @copyright value found, or empty string."""
    m = ANY_COPYRIGHT_RE.search(text)
    return m.group(1).strip() if m else ""


def check_file(path: Path, rel: str) -> list[str]:
    """Return a list of violation messages for *path* (empty == clean)."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return [f"Cannot read file: {exc}"]

    errors: list[str] = []
    is_renesas = bool(RENESAS_MARKER_RE.search(text))

    if is_renesas:
        if not RENESAS_COPYRIGHT_RE.search(text):
            found = find_existing_copyright(text)
            if found:
                errors.append(
                    f"Wrong @copyright for Renesas-derived file\n"
                    f"    found:    @copyright {found}\n"
                    f"    expected: @copyright Copyright (c) 2026 Locked Inc."
                    f" Based on Renesas Electronics Corporation source."
                )
            else:
                errors.append(
                    "Missing @copyright in Renesas-derived file\n"
                    "    expected: @copyright Copyright (c) 2026 Locked Inc."
                    " Based on Renesas Electronics Corporation source."
                )
    else:
        if not SPDX_RE.search(text):
            errors.append("Missing: SPDX-License-Identifier: MIT")

        if not OUR_COPYRIGHT_RE.search(text):
            found = find_existing_copyright(text)
            if found:
                errors.append(
                    f"Wrong @copyright\n"
                    f"    found:    @copyright {found}\n"
                    f"    expected: @copyright Copyright (c) 2026 Locked Inc."
                )
            else:
                errors.append(
                    "Missing @copyright\n"
                    "    expected: @copyright Copyright (c) 2026 Locked Inc."
                )

    return errors


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
    repo_root = Path(
        subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True, text=True,
        ).stdout.strip()
    )

    if len(sys.argv) > 1:
        paths = [Path(p) for p in sys.argv[1:]]
        # Convert to relative paths for exempt check
        candidates = []
        for p in paths:
            try:
                rel = str(p.resolve().relative_to(repo_root))
            except ValueError:
                rel = str(p)
            if p.suffix in SOURCE_EXTENSIONS and not is_exempt(rel):
                candidates.append(p)
    else:
        candidates = git_tracked_source_files(repo_root)

    failures: dict[str, list[str]] = {}
    for path in candidates:
        try:
            rel = str(path.resolve().relative_to(repo_root))
        except ValueError:
            rel = str(path)
        errs = check_file(path, rel)
        if errs:
            failures[rel] = errs

    if not failures:
        print(f"[OK] Copyright headers correct in all {len(candidates)} checked file(s).")
        return 0

    print(f"[FAIL] Copyright violations found in {len(failures)} file(s):\n")
    for rel, errs in sorted(failures.items()):
        print(f"  {rel}")
        for e in errs:
            for line in e.splitlines():
                print(f"    {line}")
        print()

    print("Required formats:")
    print("  Locked Inc. files:   SPDX-License-Identifier: MIT")
    print("                       @copyright Copyright (c) 2026 Locked Inc.")
    print("  Renesas-derived:     @copyright Copyright (c) 2026 Locked Inc."
          " Based on Renesas Electronics Corporation source.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
