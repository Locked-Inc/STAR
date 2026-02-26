#!/usr/bin/env python3
"""Fix unescaped brackets inside #doc-xxx[...] content blocks in Typst files.

Uses line-structure heuristics to identify multi-line doc blocks:
- A doc block starts with #doc-xxx[
- It ends when the NEXT line starts with # or is blank/structural
- All [ and ] within the content (except opening [ and closing ]) are escaped
"""
import re
import sys

DOC_TAGS = ['#doc-postcondition[', '#doc-precondition[', '#doc-note[', '#doc-warning[']


def is_structural_line(line):
    """Check if a line starts a new structural element (not continuation of doc block)."""
    s = line.strip()
    if not s:
        return True
    if s.startswith('#'):
        return True
    if s.startswith('*') and '::*' in s:
        return True  # *Thread Safety::* etc
    if s.startswith('_Since '):
        return True
    if s.startswith('==='):
        return True
    if s.startswith('---'):
        return True
    if s.startswith('\\_Defined '):
        return True
    return False


def escape_brackets(text):
    """Escape [ and ] in text, preserving already-escaped ones."""
    result = []
    i = 0
    while i < len(text):
        if text[i] == '\\' and i + 1 < len(text) and text[i+1] in '[]':
            result.append(text[i:i+2])
            i += 2
        elif text[i] in '[]':
            result.append('\\' + text[i])
            i += 1
        else:
            result.append(text[i])
            i += 1
    return ''.join(result)


def fix_file(path):
    with open(path, 'r') as f:
        lines = f.readlines()

    result = []
    i = 0
    fixes = 0

    while i < len(lines):
        line = lines[i]

        # Check if line starts a doc block
        tag_found = None
        for tag in DOC_TAGS:
            if line.lstrip().startswith(tag):
                tag_found = tag
                break

        if tag_found is None:
            # Also check for brackets inside #param-table and #retval-table content
            # These are handled separately
            result.append(line)
            i += 1
            continue

        # Found a doc block opening
        # Determine if this is single-line or multi-line
        # by checking if the NEXT line is a continuation

        # Check: does the line end with ] and is the next line structural?
        stripped = line.rstrip('\n')
        next_line = lines[i + 1] if i + 1 < len(lines) else ''

        if stripped.endswith(']') and is_structural_line(next_line):
            # Single-line block - escape inner brackets
            # Find the tag in the line
            tag_pos = line.index(tag_found)
            prefix = line[:tag_pos]
            after_tag = line[tag_pos + len(tag_found):]

            # Content is everything after the tag up to the last ]
            # The last ] is the closing bracket
            if after_tag.rstrip('\n').endswith(']'):
                content = after_tag.rstrip('\n')[:-1]  # Remove trailing ]
                newline = after_tag[len(after_tag.rstrip('\n')):]  # Preserve \n
                escaped_content = escape_brackets(content)
                result.append(prefix + tag_found + escaped_content + ']' + newline)
                if escaped_content != content:
                    fixes += 1
            else:
                result.append(line)
            i += 1
        elif not is_structural_line(next_line):
            # Multi-line block
            # Collect all continuation lines until we find a structural line
            block_lines = [line]
            j = i + 1
            while j < len(lines):
                if is_structural_line(lines[j]):
                    break
                block_lines.append(lines[j])
                j += 1

            # The block content is:
            # First line: everything after #doc-xxx[
            # Middle lines: full content
            # Last line: content up to the final ]

            tag_pos = block_lines[0].index(tag_found)
            prefix = block_lines[0][:tag_pos]
            first_content = block_lines[0][tag_pos + len(tag_found):]

            # Build full content
            all_content_parts = [first_content]
            for bl in block_lines[1:]:
                all_content_parts.append(bl)

            full_content = ''.join(all_content_parts)

            # The closing ] should be at the end of the last line
            # Strip trailing newline, check for ]
            full_stripped = full_content.rstrip('\n')
            trailing_nl = full_content[len(full_stripped):]

            if full_stripped.endswith(']'):
                inner = full_stripped[:-1]
                escaped_inner = escape_brackets(inner)
                result.append(prefix + tag_found + escaped_inner + ']' + trailing_nl)
                if escaped_inner != inner:
                    fixes += 1
            else:
                # No closing ] found - add one and escape everything
                escaped = escape_brackets(full_stripped)
                result.append(prefix + tag_found + escaped + ']' + trailing_nl)
                fixes += 1

            i = j
        else:
            # Single line that ends with ] and next IS structural
            # Already handled above, but just in case
            result.append(line)
            i += 1

    with open(path, 'w') as f:
        f.writelines(result)

    print(f'Fixed {fixes} doc blocks in: {path}')


if __name__ == '__main__':
    for p in sys.argv[1:]:
        fix_file(p)
