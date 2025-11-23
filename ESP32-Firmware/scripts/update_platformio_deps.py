#!/usr/bin/env python3
"""
Auto-update platformio.ini with library dependencies for sensor examples.

Reads the dependency analysis from example_deps_config.ini and automatically
updates platformio.ini to add lib_deps declarations to sensor examples.
"""

import re
from pathlib import Path

def read_dependency_mapping(config_file):
    """
    Parse example_deps_config.ini to extract dependency mappings.

    Returns:
        dict: Mapping of example names to their required libraries
    """
    deps_map = {}
    current_example = None
    current_deps = []

    with open(config_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()

            # Match environment section headers
            env_match = re.match(r'\[env:example_(\d+_\w+)\]', line)
            if env_match:
                if current_example and current_deps:
                    deps_map[current_example] = current_deps
                current_example = env_match.group(1)
                current_deps = []
                continue

            # Match library dependencies (skip the inherit line)
            if current_example and line.startswith('lib/star_'):
                current_deps.append(line)

    # Add last example
    if current_example and current_deps:
        deps_map[current_example] = current_deps

    return deps_map

def update_platformio_ini(platformio_file, deps_map):
    """
    Update platformio.ini file with lib_deps for sensor examples.

    Args:
        platformio_file: Path to platformio.ini
        deps_map: Dictionary mapping example names to their dependencies
    """
    with open(platformio_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    updated_lines = []
    i = 0
    updates_count = 0
    skipped_count = 0

    while i < len(lines):
        line = lines[i]

        # Check if this is an example environment
        env_match = re.match(r'\[env:example_(\d+_\w+)\]', line.strip())

        if env_match:
            example_name = env_match.group(1)
            updated_lines.append(line)
            i += 1

            # Check if this example needs dependencies
            if example_name in deps_map:
                # Check if it already has lib_deps
                has_lib_deps = False
                start_pos = len(updated_lines)

                # Look ahead to see if lib_deps already exists
                j = i
                while j < len(lines) and not lines[j].strip().startswith('['):
                    if lines[j].strip().startswith('lib_deps'):
                        has_lib_deps = True
                        break
                    j += 1

                if has_lib_deps:
                    # Skip this example, it already has lib_deps
                    print(f"  SKIP: {example_name} (already has lib_deps)")
                    skipped_count += 1
                else:
                    # Add lib_deps after the extends line
                    print(f"  UPDATE: {example_name} -> {', '.join([d.split('/')[-1] for d in deps_map[example_name]])}")

                    # Insert lib_deps declaration
                    updated_lines.append("lib_deps =\n")
                    updated_lines.append("    ${env:esp32_wroom.lib_deps}\n")
                    for dep in deps_map[example_name]:
                        updated_lines.append(f"    {dep}\n")
                    updates_count += 1
        else:
            updated_lines.append(line)
            i += 1

    # Write updated platformio.ini
    with open(platformio_file, 'w', encoding='utf-8') as f:
        f.writelines(updated_lines)

    return updates_count, skipped_count

def main():
    """Main entry point."""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    config_file = script_dir / 'example_deps_config.ini'
    platformio_file = project_root / 'platformio.ini'

    if not config_file.exists():
        print(f"ERROR: {config_file} not found!")
        print("Run analyze_example_deps.py first to generate the dependency config.")
        return 1

    if not platformio_file.exists():
        print(f"ERROR: {platformio_file} not found!")
        return 1

    print("=" * 80)
    print("Auto-updating platformio.ini with sensor example dependencies")
    print("=" * 80)
    print()

    # Read dependency mappings
    print("Reading dependency mappings from example_deps_config.ini...")
    deps_map = read_dependency_mapping(config_file)
    print(f"Found {len(deps_map)} examples with dependencies\n")

    # Backup original file
    backup_file = platformio_file.with_suffix('.ini.backup')
    print(f"Creating backup: {backup_file}")
    with open(platformio_file, 'r', encoding='utf-8') as src:
        with open(backup_file, 'w', encoding='utf-8') as dst:
            dst.write(src.read())

    # Update platformio.ini
    print("\nUpdating platformio.ini...")
    print()
    updates_count, skipped_count = update_platformio_ini(platformio_file, deps_map)

    print()
    print("=" * 80)
    print("Update Complete!")
    print("=" * 80)
    print(f"  Updated: {updates_count} examples")
    print(f"  Skipped: {skipped_count} examples (already had lib_deps)")
    print(f"  Total:   {len(deps_map)} sensor examples")
    print()
    print(f"Original file backed up to: {backup_file}")
    print(f"Updated file: {platformio_file}")
    print()
    print("You can now run your build scripts and they will use minimal dependencies!")

    return 0

if __name__ == '__main__':
    exit(main())
