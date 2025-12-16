#!/usr/bin/env python3
"""
Convert cflow output to DOT format for graph visualization
This creates call graphs from cflow analysis
"""

import sys
import re
import argparse

def parse_cflow_line(line):
    """Parse a cflow line and extract function name and indentation level"""
    # Remove ANSI color codes if present
    line = re.sub(r'\x1b\[[0-9;]*m', '', line)
    
    # Count indentation level
    level = 0
    for char in line:
        if char == ' ':
            level += 1
        elif char == '\t':
            level += 4  # Treat tab as 4 spaces
        else:
            break
    
    # Extract function name (everything before the first '(' or '<')
    match = re.search(r'(\w+)\s*[(<]', line.strip())
    if match:
        func_name = match.group(1)
        return level, func_name
    return None, None

def cflow_to_dot(cflow_input, output_file=None, title="Call Graph"):
    """Convert cflow output to DOT format"""
    
    dot_content = [
        f'digraph "{title}" {{',
        '    rankdir=LR;',
        '    node [shape=box style=filled fillcolor=lightblue fontname="Arial"];',
        '    edge [color=navy];',
        '    '
    ]
    
    functions = set()
    edges = []
    call_stack = []  # Stack to track calling hierarchy
    
    for line_num, line in enumerate(cflow_input):
        level, func_name = parse_cflow_line(line)
        
        if func_name is None:
            continue
            
        functions.add(func_name)
        
        # Adjust call stack based on indentation level
        while len(call_stack) > 0 and call_stack[-1][0] >= level:
            call_stack.pop()
        
        # If we have a caller, add an edge
        if call_stack:
            caller = call_stack[-1][1]
            edges.append(f'    "{caller}" -> "{func_name}";')
        
        # Push current function onto stack
        call_stack.append((level, func_name))
    
    # Add special styling for key functions
    for func in functions:
        if func == 'app_main':
            dot_content.append(f'    "{func}" [fillcolor=lightgreen fontweight=bold];')
        elif func.startswith('star_'):
            dot_content.append(f'    "{func}" [fillcolor=lightcyan];')
        elif func.startswith('esp_') or func.startswith('vTask'):
            dot_content.append(f'    "{func}" [fillcolor=lightyellow];')
    
    # Add all edges
    dot_content.extend(sorted(set(edges)))  # Remove duplicates and sort
    
    dot_content.extend([
        '',
        '    // Legend',
        '    subgraph cluster_legend {',
        '        label="Legend";',
        '        style=filled;',
        '        fillcolor=white;',
        '        "Entry Point" [fillcolor=lightgreen];',
        '        "STAR Functions" [fillcolor=lightcyan];', 
        '        "ESP-IDF/FreeRTOS" [fillcolor=lightyellow];',
        '        "Other Functions" [fillcolor=lightblue];',
        '    }',
        '}'
    ])
    
    dot_output = '\n'.join(dot_content)
    
    if output_file:
        with open(output_file, 'w') as f:
            f.write(dot_output)
    else:
        print(dot_output)

def main():
    parser = argparse.ArgumentParser(description='Convert cflow output to DOT format')
    parser.add_argument('input_file', nargs='?', help='Input cflow file (default: stdin)')
    parser.add_argument('-o', '--output', help='Output DOT file (default: stdout)')
    parser.add_argument('-t', '--title', default='Call Graph', help='Graph title')
    
    args = parser.parse_args()
    
    if args.input_file:
        with open(args.input_file, 'r') as f:
            cflow_input = f.readlines()
    else:
        cflow_input = sys.stdin.readlines()
    
    cflow_to_dot(cflow_input, args.output, args.title)

if __name__ == '__main__':
    main()