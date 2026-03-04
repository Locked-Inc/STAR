#!/usr/bin/env python3
"""Filter Graphviz DOT graphs by node name patterns.

Reads a DOT digraph from stdin and writes a filtered graph to stdout.
Keeps edges where at least one endpoint matches the include pattern,
and preserves graph attributes and styling.

Usage:
    python3 filter_dot.py --include-pattern="rx_pid_|internal_rx_pid_" < full.dot > filtered.dot
    python3 filter_dot.py --exclude-pattern="^tx_|^pb_" < full.dot > filtered.dot
"""

import argparse
import re
import sys


def parse_dot(input_stream):
    """Parse a DOT file into structural components.

    Returns:
        tuple: (header_lines, node_lines, edge_lines, footer_lines, supplementary_lines)
    """
    header_lines = []
    node_lines = []
    edge_lines = []
    supplementary_lines = []
    footer_lines = []

    in_header = True
    node_pattern = re.compile(r'^\s*"([^"]+)"\s*\[')
    edge_pattern = re.compile(r'^\s*"([^"]+)"\s*->\s*"([^"]+)"')
    comment_pattern = re.compile(r'^\s*/\*.*\*/\s*$')

    for line in input_stream:
        line = line.rstrip("\n")

        # Detect closing brace
        if line.strip() == "}":
            footer_lines.append(line)
            continue

        # Check for edges
        edge_match = edge_pattern.match(line)
        if edge_match:
            in_header = False
            edge_lines.append((line, edge_match.group(1), edge_match.group(2)))
            continue

        # Check for nodes
        node_match = node_pattern.match(line)
        if node_match:
            in_header = False
            node_lines.append((line, node_match.group(1)))
            continue

        # Comments between nodes/edges
        if comment_pattern.match(line) and not in_header:
            supplementary_lines.append(line)
            continue

        # Everything else is header
        if in_header:
            header_lines.append(line)
        else:
            supplementary_lines.append(line)

    return header_lines, node_lines, edge_lines, footer_lines, supplementary_lines


def filter_graph(header_lines, node_lines, edge_lines, footer_lines,
                 supplementary_lines, include_pattern=None,
                 exclude_pattern=None):
    """Filter a parsed DOT graph by node name patterns.

    Args:
        include_pattern: Regex - keep nodes/edges matching this pattern
        exclude_pattern: Regex - remove nodes/edges matching this pattern

    Returns:
        str: Filtered DOT graph source
    """
    include_re = re.compile(include_pattern) if include_pattern else None
    exclude_re = re.compile(exclude_pattern) if exclude_pattern else None

    def node_matches(name):
        if include_re and not include_re.search(name):
            return False
        if exclude_re and exclude_re.search(name):
            return False
        return True

    # Determine which nodes to keep based on patterns
    # First pass: find directly matching nodes
    matching_nodes = set()
    for _, name in node_lines:
        if node_matches(name):
            matching_nodes.add(name)

    # Find edges where at least one endpoint matches
    kept_edges = []
    connected_nodes = set()
    for line, src, dst in edge_lines:
        src_matches = src in matching_nodes
        dst_matches = dst in matching_nodes

        # Keep edge if at least one endpoint matches
        if src_matches or dst_matches:
            kept_edges.append(line)
            connected_nodes.add(src)
            connected_nodes.add(dst)

    # Keep nodes that are either matching or connected via kept edges
    kept_nodes = matching_nodes | connected_nodes

    # Build output
    lines = []
    lines.extend(header_lines)

    # Emit kept nodes
    lines.append("")
    lines.append("  /* Filtered nodes */")
    for line, name in node_lines:
        if name in kept_nodes:
            lines.append(line)

    # Emit kept edges
    lines.append("")
    lines.append("  /* Filtered edges */")
    for line in kept_edges:
        lines.append(line)

    lines.extend(footer_lines)

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Filter Graphviz DOT graphs by node name patterns"
    )
    parser.add_argument(
        "--include-pattern", default=None,
        help="Regex pattern - keep nodes matching this (and their connections)"
    )
    parser.add_argument(
        "--exclude-pattern", default=None,
        help="Regex pattern - remove nodes matching this"
    )

    args = parser.parse_args()

    if not args.include_pattern and not args.exclude_pattern:
        # No filtering - pass through
        sys.stdout.write(sys.stdin.read())
        return

    header, nodes, edges, footer, supplementary = parse_dot(sys.stdin)

    result = filter_graph(
        header, nodes, edges, footer, supplementary,
        include_pattern=args.include_pattern,
        exclude_pattern=args.exclude_pattern,
    )

    total_kept = result.count("->")
    total_original = len(edges)
    print(
        f"/* Filtered: {total_kept}/{total_original} edges kept */",
        file=sys.stderr,
    )

    print(result)


if __name__ == "__main__":
    main()
