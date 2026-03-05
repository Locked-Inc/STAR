#!/usr/bin/env python3
"""check-since-version.py - Enforce @since version ceiling for STAR firmware.

Every @since tag in C/C++ source files under star-rx72n-firmware/ must reference
a version that does not exceed the version declared in star-rx72n-firmware/VERSION.

Usage:
    python3 scripts/utils/check-since-version.py              # scan all git-tracked files
    python3 scripts/utils/check-since-version.py file1 ...    # scan specific files
"""

import re
import subprocess
import sys
from pathlib import Path

EXEMPT_PREFIXES = (
    "star-rx72n-firmware/libs/rx_nanopb/nanopb/",
    "star-rx72n-firmware/libs/rx_nanopb/inc/gen/",
    "star-rx72n-firmware/libs/threadx/",
    "star-rx72n-firmware/build/",
)

SINCE_RE = re.compile(r"@since\s+[Vv]ersion\s+(\d+\.\d+\.\d+)", re.MULTILINE)

# Files containing this marker are Renesas-derived; their @since tags use
# Renesas versioning, not Locked Inc. versioning, so they are skipped.
RENESAS_MARKER_RE = re.compile(
    r"Copyright.*Renesas Electronics Corporation",
    re.IGNORECASE,
)


def parse_version(v: str) -> tuple[int, int, int]:
    parts = v.strip().split(".")
    return (int(parts[0]), int(parts[1]), int(parts[2]))


def check_file(path: Path, max_version: tuple[int, int, int]) -> list[str]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return [f"Cannot read file: {exc}"]

    # Skip Renesas-derived files -- their @since tags are Renesas versioning
    if RENESAS_MARKER_RE.search(text):
        return []

    errors = []
    for m in SINCE_RE.finditer(text):
        ver_str = m.group(1)
        try:
            ver = parse_version(ver_str)
        except ValueError:
            errors.append(f"Unparseable @since version: {ver_str!r}")
            continue
        if ver > max_version:
            max_str = ".".join(str(x) for x in max_version)
            errors.append(
                f"@since Version {ver_str} exceeds max allowed {max_str}"
            )
    return errors


def is_exempt(rel: str) -> bool:
    return any(rel.startswith(p) for p in EXEMPT_PREFIXES)


def main() -> int:
    repo_root = Path(
        subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True, text=True,
        ).stdout.strip()
    )

    version_file = repo_root / "star-rx72n-firmware" / "VERSION"
    try:
        max_version = parse_version(version_file.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print(f"[FAIL] Cannot read star-rx72n-firmware/VERSION: {exc}")
        return 1

    max_str = ".".join(str(x) for x in max_version)

    if len(sys.argv) > 1:
        candidates: list[tuple[Path, str]] = []
        for a in sys.argv[1:]:
            p = Path(a)
            try:
                rel = str(p.resolve().relative_to(repo_root))
            except ValueError:
                rel = str(p)
            if (rel.startswith("star-rx72n-firmware/")
                    and p.suffix in {".c", ".h"}
                    and not is_exempt(rel)):
                candidates.append((p, rel))
    else:
        result = subprocess.run(
            ["git", "ls-files", "--", "star-rx72n-firmware/*.c",
             "star-rx72n-firmware/*.h",
             "star-rx72n-firmware/**/*.c",
             "star-rx72n-firmware/**/*.h"],
            capture_output=True, text=True, cwd=repo_root,
        )
        candidates = []
        for line in result.stdout.splitlines():
            rel = line.strip()
            if rel and not is_exempt(rel):
                candidates.append((repo_root / rel, rel))

    failures: dict[str, list[str]] = {}
    for path, rel in candidates:
        errs = check_file(path, max_version)
        if errs:
            failures[rel] = errs

    if not failures:
        print(f"[OK] All @since versions <= {max_str} in {len(candidates)} checked file(s).")
        return 0

    print(f"[FAIL] @since version violations in {len(failures)} file(s):\n")
    for rel, errs in sorted(failures.items()):
        print(f"  {rel}")
        for e in errs:
            print(f"    {e}")
        print()
    print(f"Max allowed version: {max_str}  (from star-rx72n-firmware/VERSION)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
