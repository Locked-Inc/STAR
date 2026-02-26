#!/usr/bin/env python3
"""Generate a Typst document from Go package documentation.

Walks all Go packages in star-gateway and extracts documentation
using `go doc -all`, formatting it into a compilable Typst file.
"""

import os
import re
import subprocess
import sys

GATEWAY_DIR = os.path.join(os.path.dirname(__file__), "..", "star-gateway")


def find_go_packages(root):
    """Find all directories containing .go files (excluding tests/vendor)."""
    packages = []
    for dirpath, dirnames, filenames in os.walk(root):
        # Skip hidden, vendor, testdata
        dirnames[:] = [
            d for d in dirnames
            if not d.startswith(".") and d not in ("vendor", "testdata")
        ]
        go_files = [f for f in filenames if f.endswith(".go") and not f.endswith("_test.go")]
        if go_files:
            rel = os.path.relpath(dirpath, root)
            if rel == ".":
                rel = "."
            else:
                rel = "./" + rel
            packages.append((rel, dirpath))
    return sorted(packages)


def get_go_doc(pkg_path, gateway_dir):
    """Run go doc -all on a package and return the output."""
    try:
        result = subprocess.run(
            ["go", "doc", "-all", pkg_path],
            capture_output=True, text=True, cwd=gateway_dir,
            env={**os.environ, "GOWORK": "off"},
            timeout=30
        )
        return result.stdout
    except Exception as e:
        return f"Error: {e}"


def escape_typst(text):
    """Escape special Typst characters in text."""
    # Escape characters that have special meaning in Typst
    text = text.replace("\\", "\\\\")
    text = text.replace("#", "\\#")
    text = text.replace("$", "\\$")
    text = text.replace("@", "\\@")
    text = text.replace("<", "\\<")
    text = text.replace(">", "\\>")
    text = text.replace("_", "\\_")
    text = text.replace("*", "\\*")
    return text


def format_doc_as_typst(pkg_rel, doc_text):
    """Convert go doc output to Typst markup."""
    lines = doc_text.split("\n")
    output = []
    in_code = False

    for line in lines:
        stripped = line.rstrip()

        # Detect code blocks (indented by 4+ spaces in go doc output)
        if stripped and line.startswith("    ") and not line.startswith("     "):
            if not in_code:
                output.append("```go")
                in_code = True
            output.append(stripped)
        elif in_code and (not stripped or line.startswith("    ")):
            if stripped:
                output.append(stripped)
            else:
                output.append("")
        else:
            if in_code:
                output.append("```")
                in_code = False

            if not stripped:
                output.append("")
            elif stripped.startswith("func ") or stripped.startswith("method "):
                output.append(f"\n=== {escape_typst(stripped)}\n")
            elif stripped.startswith("type "):
                output.append(f"\n=== {escape_typst(stripped)}\n")
            elif stripped.startswith("var ") or stripped.startswith("const "):
                output.append(f"\n=== {escape_typst(stripped)}\n")
            elif stripped.startswith("CONSTANTS") or stripped.startswith("VARIABLES"):
                output.append(f"\n=== {stripped}\n")
            elif stripped.startswith("TYPES") or stripped.startswith("FUNCTIONS"):
                output.append(f"\n=== {stripped}\n")
            elif stripped.startswith("package "):
                # Skip the "package X // import ..." line, we handle it as heading
                pass
            elif stripped.startswith("Package "):
                output.append(escape_typst(stripped))
            else:
                output.append(escape_typst(stripped))

    if in_code:
        output.append("```")

    return "\n".join(output)


def main():
    gateway_dir = os.path.abspath(GATEWAY_DIR)
    packages = find_go_packages(gateway_dir)

    out = []
    out.append('#set page(paper: "us-letter", margin: (x: 1in, y: 1in))')
    out.append('#set text(font: "New Computer Modern", size: 10pt)')
    out.append('#set heading(numbering: "1.1")')
    out.append('#show link: underline')
    out.append('#show raw: set text(size: 8pt)')
    out.append("")
    out.append('#align(center)[')
    out.append('  #text(size: 24pt, weight: "bold")[STAR Gateway API Reference]')
    out.append('  #v(0.5em)')
    out.append('  #text(size: 14pt)[Go Package Documentation]')
    out.append('  #v(0.5em)')
    out.append('  #text(size: 11pt)[February 2026]')
    out.append(']')
    out.append('#v(2em)')
    out.append('#outline(depth: 3)')
    out.append('#pagebreak()')
    out.append("")

    for pkg_rel, pkg_abs in packages:
        # Create a nice package name
        pkg_name = pkg_rel.replace("./", "").replace("/", ".")
        if pkg_name == ".":
            pkg_name = "star-gateway (root)"

        doc_text = get_go_doc(pkg_rel, gateway_dir)
        if not doc_text.strip():
            continue

        out.append(f"= Package: {escape_typst(pkg_name)}")
        out.append("")
        out.append(format_doc_as_typst(pkg_rel, doc_text))
        out.append("")
        out.append("#pagebreak()")
        out.append("")

    output_path = os.path.join(
        os.path.dirname(__file__), "..", "docs", "gateway_api.typ"
    )
    with open(output_path, "w") as f:
        f.write("\n".join(out))

    print(f"Generated {output_path}")
    print(f"Packages documented: {len(packages)}")


if __name__ == "__main__":
    main()
