#!/usr/bin/env python3
"""Fix Typst syntax issues in star-docs generated API files."""
import re
import sys


def fix_doc_block_content(content):
    """Escape special chars inside #doc-xxx[...] content."""
    content = content.replace('*', '\\*')
    content = content.replace('[', '\\[')
    content = content.replace(']', '\\]')
    return content


def process_doc_blocks(line):
    """Fix all #doc-xxx[...] blocks in a line."""
    for tag in ['doc-postcondition', 'doc-precondition', 'doc-note', 'doc-warning']:
        pattern = f'#{tag}['
        while pattern in line:
            idx_start = line.index(pattern) + len(pattern)
            # Find matching ] by counting brackets
            depth = 1
            idx = idx_start
            while idx < len(line) and depth > 0:
                ch = line[idx]
                if ch == '[' and (idx == 0 or line[idx-1] != '\\'):
                    depth += 1
                elif ch == ']' and (idx == 0 or line[idx-1] != '\\'):
                    depth -= 1
                idx += 1
            if depth == 0:
                inner = line[idx_start:idx-1]
                fixed_inner = fix_doc_block_content(inner)
                line = line[:idx_start] + fixed_inner + ']' + line[idx:]
            else:
                # Unclosed bracket - escape the content to end of line
                inner = line[idx_start:]
                fixed_inner = fix_doc_block_content(inner)
                line = line[:idx_start] + fixed_inner + ']'
            break  # Process one block per iteration to avoid index issues
    return line


def fix_file(path):
    with open(path, "r") as f:
        content = f.read()

    # Pre-pass: Fix LaTeX math
    content = re.sub(
        r'\\\$\s*([^\\]*?)\s*\\\$',
        lambda m: '$' + re.sub(r'[_^]\{([^}]*)\}', r'\1', m.group(1)).replace('\\times', 'x').replace('\\frac', '').replace('\\cdot', '.') + '$',
        content
    )
    content = re.sub(r'\\\[([^]]*?)\\\]', lambda m: '$ ' + m.group(1).strip() + ' $', content)
    content = content.replace('\\$', '$')
    content = re.sub(r'(\w)_\{([^}]*)\}', r'\1_\2', content)
    content = re.sub(r'(\w)\^\{([^}]*)\}', r'\1^\2', content)
    content = content.replace('\\frac', '')
    content = content.replace('\\times', 'x')

    # Process line by line
    lines = content.split('\n')
    fixed = []
    in_raw = False

    for line in lines:
        stripped = line.strip()

        if stripped.startswith('```'):
            in_raw = not in_raw
            fixed.append(line)
            continue

        if in_raw:
            fixed.append(line)
            continue

        # Lines starting with # are Typst markup
        if stripped.startswith('#'):
            line = process_doc_blocks(line)
            fixed.append(line)
            continue

        # Non-markup lines: escape ALL * outside backtick spans
        parts = re.split(r'(`[^`]*`)', line)
        new_parts = []
        for i, part in enumerate(parts):
            if i % 2 == 1:
                new_parts.append(part)
            else:
                p = part.replace('*', '\\*')
                new_parts.append(p)
        escaped = ''.join(new_parts)

        # Restore valid bold: \*Word(s):\* -> *Word(s):*
        escaped = re.sub(r'\\\*([A-Z][^\\*]*?:)\\\*', r'*\1*', escaped)
        escaped = re.sub(r'\\\*([A-Za-z]\w+)\\\*', r'*\1*', escaped)

        # Escape _ that would create subscript in non-code text
        # But NOT when it's part of _italic text_ pattern
        # Check if line has balanced _text_ (italic)
        underscore_count = len(re.findall(r'(?<!\\)_', escaped))
        if underscore_count % 2 != 0 or underscore_count == 0:
            # Unbalanced or no underscores - escape _ before uppercase
            escaped = re.sub(r'(?<=\s)_(?=[A-Z])', r'\\_', escaped)
            escaped = re.sub(r'^_(?=[A-Z])', r'\\_', escaped)

        # ASCII art: escape _ in lines with visual patterns
        if re.search(r'_{3,}|\\[_/]|/[_\\]', stripped):
            # Escape all _ not already escaped
            escaped = re.sub(r'(?<!\\)_', r'\\_', escaped)

        fixed.append(escaped)

    content = '\n'.join(fixed)

    with open(path, 'w') as f:
        f.write(content)

    print(f'Fixed: {path}')


if __name__ == '__main__':
    for p in sys.argv[1:]:
        fix_file(p)
