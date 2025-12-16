#!/usr/bin/env python3
"""
Script to automatically update KiCad fp-lib-table and sym-lib-table files.
This script scans the Symbols_and_Footprints directory and updates both tables
with all components that have KiCad subdirectories.

Usage: python3 update_library_tables.py
"""

import os
from pathlib import Path


def find_components_with_kicad(base_dir):
    """Find all component directories that contain a KiCad subdirectory."""
    components = []

    if not os.path.exists(base_dir):
        print(f"Error: Directory {base_dir} does not exist")
        return components

    for item in sorted(os.listdir(base_dir)):
        item_path = os.path.join(base_dir, item)
        kicad_path = os.path.join(item_path, "KiCad")

        if os.path.isdir(item_path) and os.path.isdir(kicad_path):
            components.append(item)

    return components


def generate_fp_lib_entry(component_name, base_dir):
    """Generate a footprint library entry for the given component."""
    kicad_path = os.path.join(base_dir, component_name, "KiCad")

    # Check if there's a .pretty subdirectory
    pretty_dirs = [d for d in os.listdir(kicad_path) if d.endswith('.pretty') and os.path.isdir(os.path.join(kicad_path, d))]

    if pretty_dirs:
        # Use the first .pretty directory found
        uri = f"${{KIPRJMOD}}/Symbols_and_Footprints/{component_name}/KiCad/{pretty_dirs[0]}"
    else:
        # Use the KiCad directory directly
        uri = f"${{KIPRJMOD}}/Symbols_and_Footprints/{component_name}/KiCad"

    return f'  (lib (name "{component_name}")(type "KiCad")(uri "{uri}")(options "")(descr ""))'


def generate_sym_lib_entry(component_name, base_dir):
    """Generate a symbol library entry for the given component."""
    kicad_path = os.path.join(base_dir, component_name, "KiCad")

    # Find all .kicad_sym files in the KiCad directory
    sym_files = [f for f in os.listdir(kicad_path) if f.endswith('.kicad_sym') and os.path.isfile(os.path.join(kicad_path, f))]

    if not sym_files:
        # No symbol file found, skip this entry or use a placeholder
        return None

    # Use the first .kicad_sym file found
    sym_file = sym_files[0]
    uri = f"${{KIPRJMOD}}/Symbols_and_Footprints/{component_name}/KiCad/{sym_file}"

    return f'  (lib (name "{component_name}")(type "KiCad")(uri "{uri}")(options "")(descr ""))'


def update_fp_lib_table(components, fp_lib_path, base_dir):
    """Update the fp-lib-table file with the given components."""
    lines = ["(fp_lib_table", "  (version 7)"]

    for component in components:
        lines.append(generate_fp_lib_entry(component, base_dir))

    lines.append(")")
    lines.append("")  # Add trailing newline

    with open(fp_lib_path, 'w') as f:
        f.write('\n'.join(lines))

    print(f"Updated {fp_lib_path} with {len(components)} components")


def update_sym_lib_table(components, sym_lib_path, base_dir):
    """Update the sym-lib-table file with the given components."""
    lines = ["(sym_lib_table", "  (version 7)"]

    skipped = []
    for component in components:
        entry = generate_sym_lib_entry(component, base_dir)
        if entry:
            lines.append(entry)
        else:
            skipped.append(component)

    lines.append(")")
    lines.append("")  # Add trailing newline

    with open(sym_lib_path, 'w') as f:
        f.write('\n'.join(lines))

    print(f"Updated {sym_lib_path} with {len(components) - len(skipped)} components")
    if skipped:
        print(f"Skipped {len(skipped)} components without .kicad_sym files: {', '.join(skipped)}")


def main():
    """Main function to update both library tables."""
    # Get the script directory
    script_dir = Path(__file__).parent.absolute()

    # Define paths
    symbols_footprints_dir = script_dir / "Symbols_and_Footprints"
    fp_lib_path = script_dir / "fp-lib-table"
    sym_lib_path = script_dir / "sym-lib-table"

    print(f"Scanning {symbols_footprints_dir} for components...")

    # Find all components with KiCad directories
    components = find_components_with_kicad(symbols_footprints_dir)

    if not components:
        print("No components found with KiCad directories")
        return

    print(f"Found {len(components)} components:")
    for comp in components:
        print(f"  - {comp}")

    print("\nUpdating library tables...")

    # Update both tables
    update_fp_lib_table(components, fp_lib_path, symbols_footprints_dir)
    update_sym_lib_table(components, sym_lib_path, symbols_footprints_dir)

    print("\nDone! Library tables have been updated.")


if __name__ == "__main__":
    main()
