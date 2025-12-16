#!/usr/bin/env python3
"""
Generate budget visualization chart for FTP document.

Usage: python3 scripts/generate_budget_chart.py
Output: figures/budget_static.pdf
"""

import json
import sys
from pathlib import Path

# Check for matplotlib
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
except ImportError:
    print("ERROR: matplotlib is required. Install with: pip install matplotlib")
    sys.exit(1)


def main():
    """Generate budget visualization chart."""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # Load BOM calculations
    bom_json = project_root / 'figures' / 'bom' / 'bom_calculations.json'
    if not bom_json.exists():
        print("ERROR: bom_calculations.json not found. Run calculate_bom.py first.")
        sys.exit(1)

    with open(bom_json, 'r') as f:
        data = json.load(f)

    # Extract category totals
    categories = data.get('categories', {})
    totals = data.get('totals', {})

    # Budget data
    electronics_buffered = float(totals.get('buffered_total', 0))
    mechanical = 377.25
    total_materials = electronics_buffered + mechanical

    # Labor data (from resource planning)
    labor_hours = 2888
    labor_rate = 20
    total_labor = labor_hours * labor_rate  # $57,760.00
    total_project = total_materials + total_labor

    # Category breakdown (with buffer applied)
    cat_data = {
        'Power Management': float(categories.get('Power Management', {}).get('buffered_total', 0)),
        'Battery Management': float(categories.get('Battery Management', {}).get('buffered_total', 0)),
        'Control/Interface': float(categories.get('Control/Interface', {}).get('buffered_total', 0)),
        'Mechanical': mechanical,
    }

    # Create figure with two subplots
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

    # Color scheme
    colors = ['#3498db', '#2ecc71', '#e74c3c', '#f39c12']

    # --- Pie Chart: Category Distribution ---
    labels = list(cat_data.keys())
    values = list(cat_data.values())

    wedges, texts, autotexts = ax1.pie(
        values,
        labels=labels,
        autopct=lambda pct: f'${pct/100*total_materials:.0f}\n({pct:.1f}%)',
        colors=colors,
        startangle=90,
        pctdistance=0.75,
    )
    ax1.set_title('Material Costs by Category', fontsize=14, fontweight='bold')

    # --- Bar Chart: Board-by-Board Costs ---
    boards = data.get('boards', {})
    board_names = []
    board_costs = []

    # Sort by category then name
    sorted_boards = sorted(boards.items(), key=lambda x: (x[1].get('category', ''), x[1].get('short_name', '')))
    for filename, board in sorted_boards:
        board_names.append(board.get('short_name', filename[:15]))
        board_costs.append(float(board.get('subtotal_with_buffer', 0)))

    # Add mechanical
    board_names.append('Mechanical')
    board_costs.append(mechanical)

    # Create horizontal bar chart
    y_pos = range(len(board_names))
    bar_colors = []
    for name in board_names:
        if name in ['3.3V Buck', '5V Buck', '6V Buck']:
            bar_colors.append(colors[0])  # Power Management
        elif name in ['AFE', 'Charger', 'Fuel Gauge', 'Gate Driver']:
            bar_colors.append(colors[1])  # Battery Management
        elif name == 'Mechanical':
            bar_colors.append(colors[3])  # Mechanical
        else:
            bar_colors.append(colors[2])  # Control/Interface

    bars = ax2.barh(y_pos, board_costs, color=bar_colors)
    ax2.set_yticks(y_pos)
    ax2.set_yticklabels(board_names, fontsize=9)
    ax2.set_xlabel('Cost ($)', fontsize=11)
    ax2.set_title('Cost by Board (with 1.2x Buffer)', fontsize=14, fontweight='bold')
    ax2.invert_yaxis()  # Top to bottom

    # Add value labels on bars
    for bar, cost in zip(bars, board_costs):
        width = bar.get_width()
        ax2.text(width + 5, bar.get_y() + bar.get_height()/2,
                f'${cost:.2f}', va='center', fontsize=8)

    # Set x-axis limit to 450 to fit labels
    ax2.set_xlim(0, 450)

    # Add legend
    legend_patches = [
        mpatches.Patch(color=colors[0], label='Power Management'),
        mpatches.Patch(color=colors[1], label='Battery Management'),
        mpatches.Patch(color=colors[2], label='Control/Interface'),
        mpatches.Patch(color=colors[3], label='Mechanical'),
    ]
    ax2.legend(handles=legend_patches, loc='upper right', fontsize=8)

    # Add totals text
    fig.text(0.5, 0.02,
             f'Total Materials: ${total_materials:.2f} (Electronics: ${electronics_buffered:.2f} + Mechanical: ${mechanical:.2f})',
             ha='center', fontsize=11, fontweight='bold')

    plt.tight_layout(rect=[0, 0.06, 1, 1])

    # Save figure
    output_path = project_root / 'figures' / 'budget_static.pdf'
    plt.savefig(output_path, format='pdf', bbox_inches='tight', dpi=300)
    print(f"Generated: {output_path}")

    # Also save PNG for preview
    png_path = project_root / 'figures' / 'budget_static.png'
    plt.savefig(png_path, format='png', bbox_inches='tight', dpi=150)
    print(f"Generated: {png_path}")

    plt.close()

    print(f"\nBudget Summary:")
    print(f"  Electronics (raw): ${float(totals.get('raw_total', 0)):.2f}")
    print(f"  Electronics (1.2x): ${electronics_buffered:.2f}")
    print(f"  Mechanical: ${mechanical:.2f}")
    print(f"  Total Materials: ${total_materials:.2f}")
    print(f"  Labor ({labor_hours} hrs @ ${labor_rate}/hr): ${total_labor:.2f}")
    print(f"  Total Project (excl. reserve): ${total_project:.2f}")


if __name__ == '__main__':
    main()
