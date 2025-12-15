#!/usr/bin/env python3
"""
Convert cflow output to DOT format at different abstraction levels
This creates multiple Egypt-style call graphs showing different levels of detail
"""

import sys
import re
import argparse
import os

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
    
    # Convert spaces to depth level (assuming 4 spaces per level)
    depth = level // 4
    
    # Extract function name (everything before the first '(' or '<')
    match = re.search(r'(\w+)\s*[(<]', line.strip())
    if match:
        func_name = match.group(1)
        return depth, func_name
    return None, None

def classify_function(func_name):
    """Classify function by type for styling"""
    if func_name == 'app_main':
        return 'entry'
    elif func_name.startswith('star_'):
        if 'sensor' in func_name or 'bus' in func_name:
            return 'star_core'
        else:
            return 'star_util'
    elif func_name.startswith('esp_') or func_name.startswith('vTask') or func_name.startswith('xSemaphore') or func_name.startswith('port'):
        return 'esp_idf'
    elif func_name in ['malloc', 'free', 'strlen', 'strcmp', 'strcpy', 'memset', 'snprintf']:
        return 'libc'
    elif func_name.startswith('ESP_LOG') or func_name.startswith('pdMS_TO_TICKS'):
        return 'esp_macro'
    elif func_name.startswith('gpio_') or func_name.startswith('i2c_'):
        return 'esp_driver'
    else:
        return 'other'

def get_color_for_type(func_type):
    """Get color scheme for different function types"""
    colors = {
        'entry': 'lightgreen',
        'star_core': 'lightblue', 
        'star_util': 'lightcyan',
        'esp_idf': 'lightyellow',
        'esp_driver': 'lightgray',
        'esp_macro': 'wheat',
        'libc': 'mistyrose',
        'other': 'white'
    }
    return colors.get(func_type, 'white')

def should_include_function(func_name, level_config):
    """Determine if function should be included based on level configuration"""
    func_type = classify_function(func_name)
    
    if level_config == 'application':
        # Only show app_main and direct STAR function calls
        return func_type in ['entry', 'star_core', 'star_util']
    elif level_config == 'framework':
        # Show application + ESP-IDF framework calls
        return func_type in ['entry', 'star_core', 'star_util', 'esp_idf', 'esp_driver']
    elif level_config == 'system':
        # Show everything except macros and libc
        return func_type not in ['esp_macro', 'libc']
    elif level_config == 'complete':
        # Show everything
        return True
    else:
        return True

def cflow_to_dot_level(cflow_input, max_depth=None, level_filter='complete', title="Call Graph"):
    """Convert cflow output to DOT format with depth and level filtering"""
    
    dot_content = [
        f'digraph "{title}" {{',
        '    rankdir=LR;',
        '    node [shape=box style=filled fontname="Arial" fontsize=10];',
        '    edge [color=navy fontsize=8];',
        '    '
    ]
    
    functions = set()
    edges = []
    call_stack = []  # Stack to track calling hierarchy
    
    for line_num, line in enumerate(cflow_input):
        depth, func_name = parse_cflow_line(line)
        
        if func_name is None:
            continue
            
        # Apply depth filter
        if max_depth is not None and depth > max_depth:
            continue
            
        # Apply level filter
        if not should_include_function(func_name, level_filter):
            continue
            
        functions.add(func_name)
        
        # Adjust call stack based on indentation level
        while len(call_stack) > 0 and call_stack[-1][0] >= depth:
            call_stack.pop()
        
        # If we have a caller, add an edge (but only if caller should also be included)
        if call_stack:
            caller_depth, caller = call_stack[-1]
            if should_include_function(caller, level_filter):
                edges.append(f'    "{caller}" -> "{func_name}";')
        
        # Push current function onto stack
        call_stack.append((depth, func_name))
    
    # Add styling for different function types
    for func in functions:
        func_type = classify_function(func)
        color = get_color_for_type(func_type)
        
        if func == 'app_main':
            dot_content.append(f'    "{func}" [fillcolor={color} fontweight=bold];')
        else:
            dot_content.append(f'    "{func}" [fillcolor={color}];')
    
    # Add all edges (remove duplicates and sort)
    dot_content.extend(sorted(set(edges)))
    
    # Add legend
    dot_content.extend([
        '',
        '    // Legend',
        '    subgraph cluster_legend {',
        '        label="Legend";',
        '        style=filled;',
        '        fillcolor=white;',
        '        "Entry Point" [fillcolor=lightgreen];',
        '        "STAR Core" [fillcolor=lightblue];',
        '        "STAR Utils" [fillcolor=lightcyan];',
        '        "ESP-IDF" [fillcolor=lightyellow];',
        '        "Drivers" [fillcolor=lightgray];',
        '        "System" [fillcolor=mistyrose];',
        '    }',
        '}'
    ])
    
    return '\n'.join(dot_content)

def generate_multiple_levels(cflow_file, output_dir):
    """Generate multiple DOT files at different abstraction levels"""
    
    with open(cflow_file, 'r') as f:
        cflow_input = f.readlines()
    
    # Define different level configurations
    levels = {
        'application': {
            'max_depth': 4,
            'level_filter': 'application',
            'title': 'Application Layer View',
            'description': 'Shows only your application code and STAR framework calls'
        },
        'framework': {
            'max_depth': 6, 
            'level_filter': 'framework',
            'title': 'Framework Layer View',
            'description': 'Shows application + ESP-IDF framework interactions'
        },
        'system': {
            'max_depth': 8,
            'level_filter': 'system', 
            'title': 'System Layer View',
            'description': 'Shows system calls without low-level macros and libc'
        },
        'complete': {
            'max_depth': None,
            'level_filter': 'complete',
            'title': 'Complete Call Graph',
            'description': 'Shows every function call including system internals'
        }
    }
    
    os.makedirs(output_dir, exist_ok=True)
    generated_files = []
    
    for level_name, config in levels.items():
        # Generate DOT content
        dot_content = cflow_to_dot_level(
            cflow_input, 
            config['max_depth'], 
            config['level_filter'],
            config['title']
        )
        
        # Write DOT file
        dot_file = os.path.join(output_dir, f'callgraph_{level_name}.dot')
        with open(dot_file, 'w') as f:
            f.write(dot_content)
        
        generated_files.append({
            'level': level_name,
            'dot_file': dot_file,
            'title': config['title'],
            'description': config['description']
        })
    
    return generated_files

def main():
    parser = argparse.ArgumentParser(description='Generate multi-level call graphs from cflow output')
    parser.add_argument('input_file', help='Input cflow file')
    parser.add_argument('-o', '--output-dir', default='./visual_levels', help='Output directory (default: ./visual_levels)')
    parser.add_argument('--generate-images', action='store_true', help='Also generate PNG/SVG/PDF files using dot')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.input_file):
        print(f"Error: Input file {args.input_file} not found")
        sys.exit(1)
    
    print(f"Generating multi-level call graphs from {args.input_file}")
    generated_files = generate_multiple_levels(args.input_file, args.output_dir)
    
    print(f"\nGenerated {len(generated_files)} DOT files in {args.output_dir}/:")
    
    for file_info in generated_files:
        print(f"  {file_info['level']}: {file_info['title']}")
        print(f"    -> {file_info['dot_file']}")
        print(f"    Description: {file_info['description']}")
        
        if args.generate_images:
            # Generate visual formats
            base_name = file_info['dot_file'].replace('.dot', '')
            
            for fmt in ['png', 'svg', 'pdf']:
                output_file = f"{base_name}.{fmt}"
                cmd = f"dot -T{fmt} \"{file_info['dot_file']}\" -o \"{output_file}\""
                if os.system(cmd) == 0:
                    print(f"    -> {output_file}")
                else:
                    print(f"    Failed to generate {output_file}")
        print()
    
    # Generate summary
    summary_file = os.path.join(args.output_dir, 'README.md')
    with open(summary_file, 'w') as f:
        f.write("# Multi-Level Call Graph Analysis\n\n")
        f.write("This directory contains call graphs at different abstraction levels:\n\n")
        
        for file_info in generated_files:
            f.write(f"## {file_info['title']}\n")
            f.write(f"**File:** `{os.path.basename(file_info['dot_file'])}`\n\n")
            f.write(f"{file_info['description']}\n\n")
        
        f.write("## Usage\n\n")
        f.write("- Start with `application` level for high-level architecture overview\n")
        f.write("- Use `framework` level to understand ESP-IDF integration\n") 
        f.write("- Examine `system` level for performance analysis\n")
        f.write("- Use `complete` level for detailed debugging\n\n")
    
    print(f"Summary created: {summary_file}")

if __name__ == '__main__':
    main()