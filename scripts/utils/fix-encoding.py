#!/usr/bin/env python3
"""Fix non-ASCII characters in STAR source files (one-time + self-service tool).

Usage: python3 scripts/utils/fix-encoding.py [--check] [paths...]
  --check  Detect only; exit 1 if any non-ASCII found (used by pre-commit)
  paths    Files or directories to process (defaults to whole repo)
"""
import sys
import os
import re

REPLACEMENTS = [
    # Context-sensitive multi-char patterns first (longer patterns before shorter)
    ("\u00b0C",  "degC"),    # degree Celsius
    ("\u00b0F",  "degF"),    # degree Fahrenheit
    ("\u00b5s",  "us"),      # microseconds
    ("\u00b5A",  "uA"),      # microamperes
    ("\u00b5V",  "uV"),      # microvolts
    ("\u00b5F",  "uF"),      # microfarads
    ("\u00b5W",  "uW"),      # microwatts
    ("\u00b5m",  "um"),      # micrometers
    # Single-char fallbacks
    ("\u00b5",   "u"),       # mu / micro prefix
    ("\u00b0",   "deg"),     # degree symbol
    ("\u03a9",   "Ohm"),     # Omega / Ohms
    ("\u00b2",   "^2"),      # superscript 2
    ("\u00b3",   "^3"),      # superscript 3
    ("\u00b9",   "^1"),      # superscript 1
    ("\u2074",   "^4"),      # superscript 4
    ("\u2075",   "^5"),      # superscript 5
    ("\u2076",   "^6"),      # superscript 6
    ("\u2080",   "_0"),      # subscript 0
    ("\u2081",   "_1"),      # subscript 1
    ("\u207b",   "^-"),      # superscript minus
    ("\u00d7",   "x"),       # multiplication sign
    ("\u00f7",   "/"),       # division sign
    ("\u2264",   "<="),      # less-than or equal
    ("\u2265",   ">="),      # greater-than or equal
    ("\u00b1",   "+/-"),     # plus-minus
    ("\u2248",   "~"),       # approximately equal
    ("\u2260",   "!="),      # not equal
    ("\u2212",   "-"),       # minus sign
    ("\u221e",   "inf"),     # infinity
    ("\u2208",   "in"),      # element of
    ("\u2295",   "XOR"),     # circled plus / XOR
    ("\u2308",   "ceil("),   # left ceiling
    ("\u2309",   ")"),       # right ceiling
    ("\u0394",   "Delta"),   # capital Delta
    ("\u03a3",   "Sigma"),   # capital Sigma
    ("\u03b8",   "theta"),   # lowercase theta
    ("\u03c0",   "pi"),      # lowercase pi
    ("\u03c4",   "tau"),     # lowercase tau
    ("\u03c9",   "omega"),   # lowercase omega
    ("\u2192",   "->"),      # right arrow
    ("\u2190",   "<-"),      # left arrow
    ("\u2191",   "^"),       # up arrow
    ("\u2193",   "v"),       # down arrow
    ("\u2194",   "<->"),     # left-right arrow
    ("\u2195",   "^v"),      # up-down arrow
    ("\u21c4",   "<->"),     # right-left arrow
    ("\u21d2",   "=>"),      # right double arrow
    ("\u27f9",   "=>"),      # long right double arrow
    ("\u27fa",   "<=>"),     # long left-right double arrow
    ("\u2705",   "[PASS]"),  # white heavy check mark
    ("\u274c",   "[FAIL]"),  # cross mark
    ("\u2713",   "[PASS]"),  # check mark
    ("\u2717",   "[FAIL]"),  # ballot x
    ("\u26a0",   "[WARN]"),  # warning sign
    ("\ufe0f",   ""),        # variation selector-16 (remove)
    # Box-drawing characters
    ("\u2500",   "-"),       # box drawings light horizontal
    ("\u2501",   "-"),       # box drawings heavy horizontal
    ("\u2502",   "|"),       # box drawings light vertical
    ("\u250a",   "|"),       # box drawings light quadruple dash vertical
    ("\u250c",   "+"),       # box drawings light down and right
    ("\u2510",   "+"),       # box drawings light down and left
    ("\u2514",   "+"),       # box drawings light up and right
    ("\u2518",   "+"),       # box drawings light up and left
    ("\u251c",   "+"),       # box drawings light vertical and right
    ("\u2524",   "+"),       # box drawings light vertical and left
    ("\u252c",   "+"),       # box drawings light down and horizontal
    ("\u2534",   "+"),       # box drawings light up and horizontal
    ("\u253c",   "+"),       # box drawings light vertical and horizontal
    ("\u2550",   "="),       # box drawings double horizontal
    ("\u2571",   "/"),       # box drawings light diagonal upper right to lower left
    ("\u2572",   "\\"),      # box drawings light diagonal upper left to lower right
    ("\u2588",   "#"),       # full block
    ("\u2591",   "."),       # light shade
    ("\u25b2",   "^"),       # black up-pointing triangle
    ("\u25ba",   ">"),       # black right-pointing pointer
    ("\u25bc",   "v"),       # black down-pointing triangle
    ("\u25c0",   "<"),       # black left-pointing triangle
    ("\u25c4",   "<"),       # black left-pointing pointer
    # Dashes and punctuation
    ("\u2013",   "-"),       # en dash
    ("\u2014",   "--"),      # em dash
    ("\u2022",   "-"),       # bullet
    ("\u2026",   "..."),     # horizontal ellipsis
    ("\u00a7",   "Sec."),    # section sign
    ("\u00b7",   "*"),       # middle dot
    ("\u203e",   "-"),       # overline
    ("\u00b6",   ""),        # pilcrow / paragraph sign (remove)
    # Additional superscripts / subscripts
    ("\u2070",   "^0"),      # superscript 0
    ("\u2071",   "^i"),      # superscript i
    ("\u2072",   "^2"),      # superscript 2 (alt)
    ("\u2073",   "^3"),      # superscript 3 (alt)
    ("\u2077",   "^7"),      # superscript 7
    ("\u2078",   "^8"),      # superscript 8
    ("\u2079",   "^9"),      # superscript 9
    # Greek letters (lowercase)
    ("\u03b1",   "alpha"),   # alpha
    ("\u03b2",   "beta"),    # beta
    ("\u03b3",   "gamma"),   # gamma
    ("\u03b4",   "delta"),   # delta (lowercase)
    ("\u03b5",   "epsilon"), # epsilon
    ("\u03b6",   "zeta"),    # zeta
    ("\u03b7",   "eta"),     # eta
    ("\u03ba",   "kappa"),   # kappa
    ("\u03bb",   "lambda"),  # lambda
    ("\u03bc",   "mu"),      # mu (alternate form)
    ("\u03bd",   "nu"),      # nu
    ("\u03be",   "xi"),      # xi
    ("\u03c1",   "rho"),     # rho
    ("\u03c3",   "sigma"),   # sigma (lowercase)
    ("\u03c5",   "upsilon"), # upsilon
    ("\u03c6",   "phi"),     # phi
    ("\u03c7",   "chi"),     # chi
    ("\u03c8",   "psi"),     # psi
    # More box-drawing / block chars
    ("\u25b6",   ">"),       # black right-pointing triangle (medium)
    ("\u2298",   "/"),       # circled division slash
    ("\u2011",   "-"),       # non-breaking hyphen
    # Clocks and status emoji -> ASCII
    ("\u23f3",   "[WAIT]"),  # hourglass (pending)
    ("\u23f8",   "[PAUSE]"), # double vertical bar (paused)
    # Coloured circle emoji -> ASCII status
    ("\U0001f534", "[RED]"),    # red circle
    ("\U0001f535", "[BLUE]"),   # blue circle
    ("\U0001f7e1", "[YELLOW]"), # yellow circle
    ("\U0001f7e2", "[GREEN]"),  # green circle
    # Common Unicode emoji -> remove (log/status files)
    ("\U0001f389",   ""),    # party popper
    ("\U0001f3af",   ""),    # direct hit
    ("\U0001f4a1",   ""),    # light bulb
    ("\U0001f4ca",   ""),    # bar chart
    ("\U0001f4cb",   ""),    # clipboard
    ("\U0001f4da",   ""),    # books
    ("\U0001f4dd",   ""),    # memo
    ("\U0001f4e6",   ""),    # package
    ("\U0001f50d",   ""),    # magnifying glass
    ("\U0001f525",   ""),    # fire
    ("\U0001f9f9",   ""),    # broom
    ("\U0001f527",   ""),    # wrench
    ("\U0001f680",   ""),    # rocket
    ("\U0001f9ea",   ""),    # test tube
    ("\u23ed",       ""),    # next track button
    # Additional common characters
    ("\u00a0",   " "),       # non-breaking space -> regular space
    ("\u2019",   "'"),       # right single quotation mark
    ("\u2018",   "'"),       # left single quotation mark
    ("\u201c",   '"'),       # left double quotation mark
    ("\u201d",   '"'),       # right double quotation mark
    ("\u00e9",   "e"),       # e with acute accent
    ("\u00e8",   "e"),       # e with grave accent
    ("\u00e0",   "a"),       # a with grave accent
    ("\u00e2",   "a"),       # a with circumflex
    ("\u00e4",   "a"),       # a with diaeresis
    ("\u00f6",   "o"),       # o with diaeresis
    ("\u00fc",   "u"),       # u with diaeresis
    ("\u00df",   "ss"),      # sharp s
]

SOURCE_EXTENSIONS = {".c", ".h", ".go", ".cpp", ".hpp", ".proto", ".ts", ".md"}

SKIP_DIRS = {".git", ".worktrees", "node_modules", "__pycache__", "build",
             ".cache", "vendor", "third_party", "nanopb", "dist", "gen"}


def process_file(path, check_only):
    """Process a single file for non-ASCII characters.

    Returns True if non-ASCII was found (violation detected or fixed).
    """
    try:
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
    except (UnicodeDecodeError, PermissionError):
        return False

    if all(ord(c) <= 127 for c in text):
        return False  # already clean

    if check_only:
        return True   # violation detected

    result = text
    for src, dst in REPLACEMENTS:
        result = result.replace(src, dst)

    # Catch-all: replace any remaining non-ASCII with '?'
    remaining = [c for c in result if ord(c) > 127]
    if remaining:
        unique = sorted(set(remaining))
        print(f"  WARNING: unmapped chars in {path}: "
              f"{[repr(c) for c in unique[:10]]}")
    result = re.sub(r"[^\x00-\x7F]", "?", result)

    with open(path, "w", encoding="utf-8") as f:
        f.write(result)
    return True


def collect_targets(root_path):
    """Collect all source files to process under root_path."""
    if os.path.isfile(root_path):
        return [root_path]

    targets = []
    for dp, dirs, files in os.walk(root_path):
        # Prune skip dirs in-place to prevent os.walk from descending
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]

        for f in files:
            if os.path.splitext(f)[1] in SOURCE_EXTENSIONS:
                targets.append(os.path.join(dp, f))
    return targets


def main():
    check_only = "--check" in sys.argv
    paths = [a for a in sys.argv[1:] if not a.startswith("--")] or ["."]

    violations = []
    for root_path in paths:
        for p in collect_targets(root_path):
            if process_file(p, check_only):
                violations.append(p)
                if not check_only:
                    print(f"  fixed: {p}")

    if check_only and violations:
        print("ERROR: Non-ASCII characters in:")
        for v in violations:
            print(f"  {v}")
        sys.exit(1)

    if not check_only:
        print(f"Done. {len(violations)} file(s) converted.")
    elif not violations:
        print("OK: No non-ASCII characters found.")


if __name__ == "__main__":
    main()
