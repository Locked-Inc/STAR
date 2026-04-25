#!/usr/bin/env python3
"""Extract every external reference / citation / verifiable claim from a
.tex document.

Surfaces the kinds of fact-anchors that a reviewer would expect to be
able to follow into a real source: parenthetical author-year style,
named reports, standards bodies (ADA / IEEE / ISO / NASA / MIL-STD /
DI-IPSC / RFC / DOI / arXiv), specific statistics, year ranges, URLs,
and named organizations. Output is plain text, one finding per block,
file:line cited so a human can land directly on the source line
without re-greping the document.

Usage:
    python3 scripts/utils/extract-sdd-citations.py [--target PATH ...] \\
                                                   [--require-findings]

    # Default: scan the SDD if no --target is given. Override via flag
    # or via the EXTRACT_SDD_DEFAULT_TARGET environment variable.
    EXTRACT_SDD_DEFAULT_TARGET=path/to/file.tex \\
        python3 scripts/utils/extract-sdd-citations.py

    # CI mode: exit non-zero if no findings were produced.
    python3 scripts/utils/extract-sdd-citations.py --require-findings

Exit codes:
    0  success (zero or more findings, depending on --require-findings)
    1  --require-findings was passed and no findings were produced
    2  one or more --target paths could not be opened

This is purely a finding aid; it does not check whether the cited
sources actually exist. That verification step is delegated to
research agents.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

# Width chosen to fit a typical 132-column terminal minus the 12-char
# indent prefix this script prints before each context line.
CTX_MAX_LEN = 140

# Default scan target if neither --target nor the env var is given.
ENV_DEFAULT_TARGET = "EXTRACT_SDD_DEFAULT_TARGET"
BUILTIN_DEFAULT_TARGET = (
    "final_docs/software_description_document/"
    "STAR_Software_Description_Document.tex"
)

# Each pattern -> (label, compiled regex). Patterns are intentionally
# broad to catch sloppy formatting; manual review filters false
# positives.
PATTERNS = [
    ("author-year (Word ..., YYYY)",
     re.compile(r"\([A-Z][A-Za-z .,&'-]{2,80}?,\s*(?:19|20)\d{2}\)")),
    ("author-year (Word ... YYYY)",
     re.compile(r"\([A-Z][A-Za-z .,&'-]{2,60}?\s+(?:19|20)\d{2}\)")),
    ("inline year mention",
     re.compile(r"\b(?:report|survey|study|paper|article|annual|"
                r"filing|filings)\s+(?:from|in|of)?\s*(?:19|20)\d{2}\b",
                re.IGNORECASE)),
    ("year range A through/to B",
     re.compile(r"(?:19|20)\d{2}\s*(?:through|to|-|--|\\textendash)\s*"
                r"(?:19|20)\d{2}")),
    ("named statistic",
     re.compile(r"\b(?:approximately|roughly|about|over)?\s*\d{1,3}\s*"
                r"percent\b", re.IGNORECASE)),
    ("standard / report identifier",
     re.compile(r"\b(?:ADA|ANSI|ASTM|DI-IPSC|DI-IPSC-\d+|IEC|IEEE|ISO|"
                r"JPL|MIL-STD|MISRA|NASA|RFC|NFPA|OSHA|UL)"
                r"[\s-]?(?:[0-9A-Z][\w./-]*)?\b")),
    ("DOI / arXiv / RFC token",
     re.compile(r"\b(?:doi:\s*10\.\S+|arXiv:\s*\d{4}\.\d{4,5}|"
                r"RFC\s*\d{3,5})\b", re.IGNORECASE)),
    # URL: stop at whitespace, backslash, closing brace/quote/angle
    # bracket, or trailing punctuation so consumers get clean URLs.
    ("URL",
     re.compile(r"https?://[^\s\\}\"<>.,;:)\]]+(?:[/#?][^\s\\}\"<>]*)?",
                re.IGNORECASE)),
    ("\\cite{...}, \\href{...}, \\url{...}, \\footnote{...}",
     re.compile(r"\\(?:cite|href|url|footnote)\{[^}]+\}")),
    ("named-org capitalized phrase",
     re.compile(r"\b(?:Seyfarth Shaw|Anthropic|OpenAI|Google|Microsoft|"
                r"GitHub|Renesas|Texas Instruments|Boston Dynamics|"
                r"MathWorks|Foxglove|Lichtblick|Caddy|Tailscale|"
                r"Grafana|Prometheus|JLCPCB|PCBWay|OSH Park|Ubuntu|"
                r"Canonical|OSRF|Open Robotics|Open Navigation|"
                r"Eclipse Foundation)\b")),
    ("named monetary / stats claim",
     re.compile(r"\$[\d,]+(?:\.\d+)?(?:\s*(?:million|billion|trillion|"
                r"M|B|K))?", re.IGNORECASE)),
    ("MKS unit fact (not citation, but factual)",
     re.compile(r"\\qty\{[^}]+\}\{[^}]+\}")),
]

# SKIP_LINES: anchored to start-of-line. When this matches we drop the
# entire line because nothing on it is a real external citation.
SKIP_LINES = re.compile(
    r"^\s*"
    r"(?:%"                                       # LaTeX comment
    r"|\\section\{"                                # section headers
    r"|\\subsection\{"
    r"|\\subsubsection\{"
    r"|\\paragraph\{"
    r")"
)

# INTERNAL_CITE: substring matches we want to drop *individually* from
# the per-line hits without losing the rest of the line. These
# represent intra-document references, not external citations.
INTERNAL_CITE = re.compile(
    r"\\label\{[^}]+\}"
    r"|\\ref\{[^}]+\}"
    r"|\\cite\{[^}]*sec:[^}]*\}"
)

# Punctuation we strip from the right edge of every URL match so a
# trailing "." or "," in prose does not survive into the report.
URL_TRAILING_PUNCT = ".,;:)]}>"


def extract(path: Path) -> "dict[str, list[tuple[int, str, str]]]":
    """Run all PATTERNS over the lines of `path` and return raw hits.

    Returns a mapping from pattern label to a list of (lineno,
    matched_text, full_line) tuples. Hits are NOT deduplicated -- the
    caller (main) is responsible for dedup. SKIP_LINES drops whole
    lines (comments, section headers); INTERNAL_CITE filters out
    individual matches that are intra-document references rather than
    external citations.
    """
    findings_per_pattern: "dict[str, list[tuple[int, str, str]]]" = {
        label: [] for label, _ in PATTERNS
    }
    text = path.read_text(encoding="utf-8")
    for lineno, line in enumerate(text.splitlines(), start=1):
        if SKIP_LINES.search(line):
            continue
        for label, pattern in PATTERNS:
            for m in pattern.finditer(line):
                match_text = m.group(0).strip()
                if INTERNAL_CITE.fullmatch(match_text):
                    continue
                if label == "URL":
                    match_text = match_text.rstrip(URL_TRAILING_PUNCT)
                findings_per_pattern[label].append(
                    (lineno, match_text, line.strip())
                )
    return findings_per_pattern


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Extract verifiable references (author-year cites, "
            "URLs, standards, named statistics, DOIs, RFCs, named "
            "organizations) from one or more .tex files."
        ),
    )
    parser.add_argument(
        "--target", "-t",
        action="append",
        default=[],
        metavar="PATH",
        help=(
            "Path to a .tex file to scan. May be passed multiple "
            "times. If omitted, falls back to the value of the "
            f"{ENV_DEFAULT_TARGET} environment variable, then to "
            f"{BUILTIN_DEFAULT_TARGET} relative to the repository "
            "root."
        ),
    )
    parser.add_argument(
        "--require-findings",
        action="store_true",
        help=(
            "Exit non-zero if no findings were produced. Off by "
            "default so the script obeys POSIX semantics in normal "
            "use."
        ),
    )
    return parser.parse_args(argv)


def resolve_targets(
    cli_targets: list[str], repo_root: Path
) -> list[Path]:
    if cli_targets:
        targets = cli_targets
    elif os.environ.get(ENV_DEFAULT_TARGET):
        targets = [os.environ[ENV_DEFAULT_TARGET]]
    else:
        targets = [BUILTIN_DEFAULT_TARGET]
    return [
        (Path(t) if Path(t).is_absolute() else repo_root / t)
        for t in targets
    ]


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    repo_root = Path(__file__).resolve().parents[2]
    paths = resolve_targets(args.target, repo_root)

    any_findings = False
    missing_targets: list[Path] = []
    for path in paths:
        if not path.exists():
            missing_targets.append(path)
            print(f"SKIP {path} (not found)")
            continue
        try:
            print(f"=== {path.relative_to(repo_root)} ===")
        except ValueError:
            print(f"=== {path} ===")
        results = extract(path)
        for label, hits in results.items():
            seen: set[tuple[int, str]] = set()
            deduped: list[tuple[int, str, str]] = []
            for lineno, match, ctx in hits:
                key = (lineno, match)
                if key in seen:
                    continue
                seen.add(key)
                deduped.append((lineno, match, ctx))
            if not deduped:
                continue
            any_findings = True
            print(f"\n--- {label} ({len(deduped)} hits) ---")
            for lineno, match, ctx in deduped:
                trimmed_ctx = (
                    (ctx[: CTX_MAX_LEN - 3] + "...")
                    if len(ctx) > CTX_MAX_LEN
                    else ctx
                )
                print(f"  L{lineno:4d}  {match!r}")
                print(f"          {trimmed_ctx}")
        print()

    if missing_targets:
        return 2
    if args.require_findings and not any_findings:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
