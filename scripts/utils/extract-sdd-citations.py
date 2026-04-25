#!/usr/bin/env python3
"""Extract every external reference / citation / verifiable claim from the SDD.

Surfaces the kinds of fact-anchors that a reviewer would expect to be able
to follow into a real source: parenthetical author-year style, named
reports, standards bodies (ADA / IEEE / ISO / NASA / MIL-STD / DI-IPSC /
RFC / DOI / arXiv), specific statistics ("X percent", "growing from
year A to year B"), and URLs. Output is plain text, one finding per
block, file:line cited so a human can land directly on the source line
without re-greping the document.

Usage:
    python3 scripts/utils/extract-sdd-citations.py
    python3 scripts/utils/extract-sdd-citations.py path/to/file.tex

This is purely a finding aid; it does not check whether the cited
sources actually exist. That verification step is delegated to
research agents.
"""

import re
import sys
from pathlib import Path

DEFAULT_TARGETS = [
    "final_docs/software_description_document/STAR_Software_Description_Document.tex",
]

# Each pattern -> (label, compiled regex). Patterns are intentionally broad
# to catch sloppy formatting; manual review filters false positives.
PATTERNS = [
    ("author-year (Word ..., YYYY)",
     re.compile(r"\([A-Z][A-Za-z .,&'-]{2,80}?,\s*(?:19|20)\d{2}\)")),
    ("author-year (Word ... YYYY)",
     re.compile(r"\([A-Z][A-Za-z .,&'-]{2,60}?\s+(?:19|20)\d{2}\)")),
    ("inline year mention",
     re.compile(r"\b(?:report|survey|study|paper|article|annual|filing|filings)\s+(?:from|in|of)?\s*(?:19|20)\d{2}\b",
                re.IGNORECASE)),
    ("year range A through/to B",
     re.compile(r"(?:19|20)\d{2}\s*(?:through|to|-|--|\\textendash)\s*(?:19|20)\d{2}")),
    ("named statistic",
     re.compile(r"\b(?:approximately|roughly|about|over)?\s*\d{1,3}\s*percent\b",
                re.IGNORECASE)),
    ("standard / report identifier",
     re.compile(r"\b(?:ADA|ANSI|ASTM|DI-IPSC|DI-IPSC-\d+|IEC|IEEE|ISO|JPL|MIL-STD|MISRA|NASA|RFC|NFPA|OSHA|UL)[\s-]?(?:[0-9A-Z][\w./-]*)?\b")),
    ("DOI / arXiv / RFC token",
     re.compile(r"\b(?:doi:\s*10\.\S+|arXiv:\s*\d{4}\.\d{4,5}|RFC\s*\d{3,5})\b",
                re.IGNORECASE)),
    ("URL",
     re.compile(r"https?://[^\s\\}\"<>]+", re.IGNORECASE)),
    ("\\cite{...}, \\href{...}, \\url{...}, \\footnote{...}",
     re.compile(r"\\(?:cite|href|url|footnote)\{[^}]+\}")),
    ("named-org capitalized phrase",
     re.compile(r"\b(?:Seyfarth Shaw|Anthropic|OpenAI|Google|Microsoft|GitHub|Renesas|Texas Instruments|Boston Dynamics|MathWorks|Foxglove|Lichtblick|Caddy|Tailscale|Grafana|Prometheus|JLCPCB|PCBWay|OSH Park|Ubuntu|Canonical|OSRF|Open Robotics)\b")),
    ("named monetary / stats claim",
     re.compile(r"\$[\d,]+(?:\.\d+)?(?:\s*(?:million|billion|trillion|M|B|K))?", re.IGNORECASE)),
    ("MKS unit fact (not citation, but factual)",
     re.compile(r"\\qty\{[^}]+\}\{[^}]+\}")),
]

# Patterns we explicitly skip: pure project-internal references that would
# bloat the output without giving a reviewer something to verify.
SKIP_LINES = re.compile(
    r"^\s*%"                              # LaTeX comments
    r"|\\label\{|\\ref\{|\\cite\{[^}]*sec:" # internal cross-refs masked as cite
    r"|\\section\{|\\subsection\{|\\subsubsection\{|\\paragraph\{"
)


def extract(path: Path):
    findings_per_pattern = {label: [] for label, _ in PATTERNS}
    text = path.read_text(encoding="utf-8")
    for lineno, line in enumerate(text.splitlines(), start=1):
        if SKIP_LINES.search(line):
            continue
        for label, pattern in PATTERNS:
            for m in pattern.finditer(line):
                findings_per_pattern[label].append((lineno, m.group(0).strip(), line.strip()))
    return findings_per_pattern


def main() -> int:
    targets = sys.argv[1:] if len(sys.argv) > 1 else DEFAULT_TARGETS
    repo_root = Path(__file__).resolve().parents[2]
    any_findings = False
    for rel in targets:
        path = (repo_root / rel) if not Path(rel).is_absolute() else Path(rel)
        if not path.exists():
            print(f"SKIP {path} (not found)")
            continue
        print(f"=== {path.relative_to(repo_root)} ===")
        results = extract(path)
        for label, hits in results.items():
            if not hits:
                continue
            any_findings = True
            print(f"\n--- {label} ({len(hits)} hits) ---")
            seen = set()
            for lineno, match, ctx in hits:
                key = (lineno, match)
                if key in seen:
                    continue
                seen.add(key)
                trimmed_ctx = (ctx[:140] + "...") if len(ctx) > 140 else ctx
                print(f"  L{lineno:4d}  {match!r}")
                print(f"          {trimmed_ctx}")
        print()
    return 0 if any_findings else 1


if __name__ == "__main__":
    sys.exit(main())
