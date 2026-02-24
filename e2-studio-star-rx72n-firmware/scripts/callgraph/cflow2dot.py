#!/usr/bin/env python3
"""Convert cflow POSIX-format output to Graphviz DOT format.

Reads cflow indented call tree from stdin and writes DOT digraph to stdout.
Supports module-based coloring, shape differentiation for internal/private
functions, and external function exclusion.

Usage:
    cflow --format=posix src/*.c | python3 cflow2dot.py > callgraph.dot
    cflow --format=posix src/*.c | python3 cflow2dot.py --title="Motor Control" --rankdir=LR
"""

import argparse
import re
import sys
from collections import defaultdict

# Module color map for visual grouping
MODULE_COLORS = {
    "rx_motor": "#AED6F1",       # Light blue
    "rx_pid": "#85C1E9",         # Blue
    "rx_drv8263": "#7FB3D8",     # Steel blue
    "rx_encoder": "#A9CCE3",     # Powder blue
    "rx_comm_manager": "#F9E79F", # Light yellow
    "rx_spi_comm": "#F7DC6F",    # Yellow
    "rx_usb_comm": "#F4D03F",    # Gold
    "rx_usb": "#F0E68C",         # Khaki
    "rx_frame": "#FAD7A0",       # Peach
    "rx_frame_ascii": "#FDEBD0", # Light peach
    "rx_nanopb": "#D5F5E3",     # Light mint
    "rx_crc": "#ABEBC6",        # Mint
    "rx_fec": "#82E0AA",        # Green
    "rx_harq": "#73C6B6",       # Teal
    "rx_ds18b20": "#D2B4DE",    # Light purple
    "rx_hcsr04": "#BB8FCE",     # Purple
    "rx_obstacle_detect": "#F5B7B1", # Light red
    "rx_bus": "#FADBD8",        # Pink
    "rx_core": "#D5DBDB",       # Light gray
    "rx_hal": "#E5E8E8",        # Silver
    "rx_infrastructure": "#CCD1D1", # Gray
    "rx_log": "#D5DBDB",        # Light gray
    "rx_check": "#D5DBDB",      # Light gray
    "main": "#FCF3CF",          # Cream
    "hardware_init": "#FCF3CF", # Cream
    "shared_data": "#FCF3CF",   # Cream
}

# Task entry color
TASK_COLOR = "#E8DAEF"  # Lavender

DEFAULT_COLOR = "#E8E8E8"  # Light gray


def detect_module(func_name):
    """Detect which module a function belongs to based on naming conventions."""
    # Strip internal_/priv_ prefix for module detection
    clean_name = func_name
    for prefix in ("internal_", "priv_"):
        if clean_name.startswith(prefix):
            clean_name = clean_name[len(prefix):]
            break

    # Check task entries
    if "_task_entry" in func_name or "_task_create" in func_name:
        return "task"

    # Match against known module prefixes
    # Sort by length descending to match longer prefixes first
    for mod in sorted(MODULE_COLORS.keys(), key=len, reverse=True):
        if clean_name.startswith(mod + "_") or clean_name == mod:
            return mod

    # Check source-level function names
    for name in ("main", "hardware_init", "shared_data"):
        if clean_name.startswith(name):
            return name

    return None


def get_node_color(func_name):
    """Get fill color for a function node based on its module."""
    if "_task_entry" in func_name:
        return TASK_COLOR

    module = detect_module(func_name)
    if module and module in MODULE_COLORS:
        return MODULE_COLORS[module]

    return DEFAULT_COLOR


def get_node_shape(func_name):
    """Get shape for a function node based on its visibility."""
    if func_name.startswith("internal_"):
        return "box"  # Internal functions: square box
    if func_name.startswith("priv_"):
        return "box"  # Private functions: square box
    if "_task_entry" in func_name:
        return "doubleoctagon"  # Task entries: prominent
    if func_name == "main" or func_name == "tx_application_define":
        return "house"  # Entry points: house shape
    return "box"  # Default: rectangle


def get_node_style(func_name):
    """Get style string for a function node."""
    if func_name.startswith("internal_") or func_name.startswith("priv_"):
        return "filled,dashed"
    return "filled"


def parse_cflow_line(line):
    """Parse a single cflow POSIX-format line.

    cflow POSIX format (with line numbers):
        N func_name: return_type (args), <file:line>
    or (GNU format):
        func_name() <return_type at file:line>

    Indentation (4 spaces per level) indicates call depth.
    Line numbers appear as "    N " prefix in POSIX mode.

    Returns:
        tuple: (indent_level, function_name, location) or None
    """
    if not line.strip():
        return None

    # Strip trailing whitespace
    line = line.rstrip()

    # cflow POSIX output: "    N func_name:" where N is right-justified line number
    # The prefix is exactly: spaces + digits + single space, then indented content
    # e.g., "    1 rx_pid_compute:" (level 0)
    #        "    2     internal_foo:" (level 1, 4 indent spaces)
    #        "    3         bar:" (level 2, 8 indent spaces)
    posix_match = re.match(r"^(\s*\d+) (.*)", line)
    if posix_match:
        # Remove the line-number prefix; the rest has indent + function info
        rest = posix_match.group(2)
    else:
        rest = line

    # Count leading spaces for indent level (4 spaces per level)
    stripped = rest.lstrip(" ")
    indent = len(rest) - len(stripped)
    level = indent // 4

    # Extract function name
    # POSIX format: "func_name: return_type (...), <file>"
    # GNU format:   "func_name() <return_type at file>"
    # External ref: "func_name: <>"
    func_match = re.match(r"(\w+)\s*[:(\[]", stripped)
    if not func_match:
        # Try bare function name at start of line
        func_match = re.match(r"(\w+)", stripped)
        if not func_match:
            return None

    func_name = func_match.group(1)

    # Extract location if present: "<...file:line>" or "<file line>"
    loc_match = re.search(r"<([^>]+)>", stripped)
    location = ""
    if loc_match:
        loc_text = loc_match.group(1).strip()
        # Skip empty location markers "<>"
        if loc_text:
            location = loc_text

    return (level, func_name, location)


def parse_cflow(input_stream, exclude_pattern=None, include_pattern=None):
    """Parse cflow POSIX output into a set of edges and nodes.

    Args:
        input_stream: File-like object with cflow output
        exclude_pattern: Regex pattern for functions to exclude
        include_pattern: Regex pattern for functions to include

    Returns:
        tuple: (edges set, nodes dict mapping name -> location)
    """
    exclude_re = re.compile(exclude_pattern) if exclude_pattern else None
    include_re = re.compile(include_pattern) if include_pattern else None

    edges = set()
    nodes = {}
    # Stack tracks the current call chain: [(level, func_name), ...]
    stack = []

    for line in input_stream:
        parsed = parse_cflow_line(line.rstrip("\n"))
        if parsed is None:
            continue

        level, func_name, location = parsed

        # Apply exclusion filter
        if exclude_re and exclude_re.search(func_name):
            continue

        # Apply inclusion filter
        if include_re and not include_re.search(func_name):
            continue

        # Record node
        nodes[func_name] = location

        # Maintain stack: pop entries at same or deeper level
        while stack and stack[-1][0] >= level:
            stack.pop()

        # Add edge from parent to this function
        if stack:
            parent_name = stack[-1][1]
            edge = (parent_name, func_name)
            if edge[0] != edge[1]:  # Skip self-references
                edges.add(edge)

        stack.append((level, func_name))

    return edges, nodes


def generate_dot(edges, nodes, title="Call Graph", rankdir="TB",
                 highlight_module=None):
    """Generate Graphviz DOT output from edges and nodes.

    Args:
        edges: Set of (caller, callee) tuples
        nodes: Dict mapping function name -> location
        title: Graph title
        rankdir: Graph direction (TB, LR, BT, RL)
        highlight_module: Module name to highlight with bold borders

    Returns:
        str: Complete DOT graph source
    """
    lines = []
    lines.append("digraph callgraph {")
    lines.append(f'  label="{title}";')
    lines.append("  labelloc=t;")
    lines.append('  fontname="Helvetica";')
    lines.append("  fontsize=14;")
    lines.append(f"  rankdir={rankdir};")
    lines.append("")
    lines.append("  /* Global node defaults */")
    lines.append('  node [fontname="Helvetica", fontsize=9];')
    lines.append('  edge [color="#666666", arrowsize=0.7];')
    lines.append("")

    # Group nodes by module for subgraph clustering
    module_nodes = defaultdict(list)
    for func_name in sorted(nodes.keys()):
        module = detect_module(func_name)
        module_nodes[module or "other"].append(func_name)

    # Emit nodes with attributes
    lines.append("  /* Nodes */")
    for func_name in sorted(nodes.keys()):
        color = get_node_color(func_name)
        shape = get_node_shape(func_name)
        style = get_node_style(func_name)

        attrs = [
            f'shape={shape}',
            f'style="{style}"',
            f'fillcolor="{color}"',
        ]

        # Highlight specific module
        if highlight_module:
            module = detect_module(func_name)
            if module == highlight_module:
                attrs.append('penwidth=2.0')
                attrs.append('color="#2C3E50"')

        # Add tooltip with location
        location = nodes.get(func_name, "")
        if location:
            attrs.append(f'tooltip="{location}"')

        attr_str = ", ".join(attrs)
        lines.append(f'  "{func_name}" [{attr_str}];')

    lines.append("")

    # Emit edges
    lines.append("  /* Edges */")
    for caller, callee in sorted(edges):
        lines.append(f'  "{caller}" -> "{callee}";')

    lines.append("}")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Convert cflow POSIX output to Graphviz DOT format"
    )
    parser.add_argument(
        "--title", default="RX72N Firmware Call Graph",
        help="Graph title (default: RX72N Firmware Call Graph)"
    )
    parser.add_argument(
        "--rankdir", default="TB", choices=["TB", "LR", "BT", "RL"],
        help="Graph direction (default: TB)"
    )
    parser.add_argument(
        "--exclude-pattern", default=None,
        help="Regex pattern for function names to exclude"
    )
    parser.add_argument(
        "--include-pattern", default=None,
        help="Regex pattern for function names to include (overrides exclude)"
    )
    parser.add_argument(
        "--highlight-module", default=None,
        help="Module name to highlight with bold borders"
    )

    args = parser.parse_args()

    edges, nodes = parse_cflow(
        sys.stdin,
        exclude_pattern=args.exclude_pattern,
        include_pattern=args.include_pattern,
    )

    if not nodes:
        print("/* No functions found in cflow output */", file=sys.stderr)
        print("digraph callgraph { }")
        sys.exit(0)

    print(f"/* {len(nodes)} nodes, {len(edges)} edges */", file=sys.stderr)

    dot_output = generate_dot(
        edges, nodes,
        title=args.title,
        rankdir=args.rankdir,
        highlight_module=args.highlight_module,
    )

    print(dot_output)


if __name__ == "__main__":
    main()
