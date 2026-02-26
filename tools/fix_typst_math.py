#!/usr/bin/env python3
"""Convert broken LaTeX math expressions to plain text in Typst API docs.

Replaces $ ... $ with plain text equivalents since the formulas are
informational and don't need proper typesetting in API reference docs.
"""
import re
import sys


def demathify(math_content):
    """Convert math expression to plain text."""
    s = math_content
    # Remove LaTeX commands
    s = s.replace('\\text{', '').replace('}', '')
    s = s.replace('\\left(', '(').replace('\\right)', ')')
    s = s.replace('\\left[', '[').replace('\\right]', ']')
    s = s.replace('\\frac', '')
    s = s.replace('\\times', 'x')
    s = s.replace('\\cdot', '*')
    s = s.replace('\\_', '_')
    s = s.replace('\\', '')
    # Clean up whitespace
    s = re.sub(r'\s+', ' ', s).strip()
    return s


def fix_file(path):
    with open(path, 'r') as f:
        lines = f.readlines()

    fixed = []
    in_raw = False
    fixes = 0

    for line in lines:
        stripped = line.strip()

        if stripped.startswith('```'):
            in_raw = not in_raw
            fixed.append(line)
            continue

        if in_raw:
            fixed.append(line)
            continue

        # Skip lines in #import or other Typst code
        if stripped.startswith('#import') or stripped.startswith('#set') or stripped.startswith('#show'):
            fixed.append(line)
            continue

        # Replace inline math $...$ with plain text
        # But preserve $ that are part of backtick-quoted code
        parts = re.split(r'(`[^`]*`)', line)
        new_parts = []
        for i, part in enumerate(parts):
            if i % 2 == 1:
                new_parts.append(part)
            else:
                # Replace $ ... $ (non-greedy, on same line)
                # Handle both $ expr $ and $expr$
                def replace_math(m):
                    content = m.group(1).strip()
                    plain = demathify(content)
                    return plain

                part = re.sub(r'\$\s*([^$]+?)\s*\$', replace_math, part)
                new_parts.append(part)
        new_line = ''.join(new_parts)

        if new_line != line:
            fixes += 1
        fixed.append(new_line)

    with open(path, 'w') as f:
        f.writelines(fixed)

    print(f'Converted {fixes} math expressions to text in: {path}')


if __name__ == '__main__':
    for p in sys.argv[1:]:
        fix_file(p)
