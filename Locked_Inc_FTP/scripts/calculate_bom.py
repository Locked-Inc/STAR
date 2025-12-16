#!/usr/bin/env python3
"""
BOM Calculator for FTP Document

Parses all BOM CSV files, calculates costs using the lowest price between
DigiKey and Mouser, applies a 1.2x spare parts buffer, and generates
outputs for LaTeX tables and verification.

Usage:
    python3 scripts/calculate_bom.py

Output:
    - figures/bom/bom_calculations.json - Machine-readable results
    - figures/bom/bom_verification.log - Human-readable audit trail
"""

import csv
import json
import os
import sys
from dataclasses import dataclass, field, asdict
from datetime import datetime
from decimal import Decimal, ROUND_HALF_UP, InvalidOperation
from pathlib import Path
from typing import Optional, Dict, List, Tuple


# =============================================================================
# Configuration
# =============================================================================

SPARE_PARTS_MULTIPLIER = Decimal("1.2")  # 20% buffer

# BOM file mapping: filename -> (short_name, display_name, category)
BOM_FILES = {
    'STAR_AFE_BOM.csv': ('AFE', 'Analog Front-End (AFE)', 'Battery Management'),
    'STAR_BUCK_BOOST_CHARGER_BOM.csv': ('Charger', 'Buck-Boost Charger', 'Battery Management'),
    'STAR_FUEL_GAUGE_BOM.csv': ('Fuel Gauge', 'Fuel Gauge', 'Battery Management'),
    'STAR_GATE_DRIVER_BOM.csv': ('Gate Driver', 'Gate Driver', 'Battery Management'),
    'STAR_LIDAR_BREAKOUT_BOM.csv': ('LiDAR', 'LiDAR Breakout', 'Control/Interface'),
    'STAR_MCU_BOM.csv': ('MCU', 'MCU Board', 'Control/Interface'),
    'STAR_MOTOR_DRIVER_BOM.csv': ('Motor Driver', 'Motor Driver', 'Control/Interface'),
    'STAR_P3V3_BUCK_BOM.csv': ('3.3V Buck', '3.3V Buck Regulator', 'Power Management'),
    'STAR_P5V0_BUCK_BOM.csv': ('5V Buck', '5V Buck Regulator', 'Power Management'),
    'STAR_P6V0_BUCK_BOM.csv': ('6V Buck', '6V Buck Regulator', 'Power Management'),
    'STAR_USB-C_BOM.csv': ('USB-C', 'USB-C PD Controller', 'Control/Interface'),
}

# CSV column names (based on actual CSV structure)
COL_REFERENCE = 'Reference'
COL_VALUE = 'Value'
COL_QUANTITY = 'QUANTITY'
COL_DNP = 'DNP'
COL_DIGIKEY_PRICE = 'DigiKey Price per Unit'
COL_MOUSER_PRICE = 'Mouser Price per Unit'
COL_MPN = 'Manufactaurer Part Number'  # Note: typo in source CSV
COL_DIGIKEY_PN = 'DigiKey Part Number'
COL_MOUSER_PN = 'Mouser Part Number'


# =============================================================================
# Data Classes
# =============================================================================

@dataclass
class Component:
    """Represents a single component from the BOM."""
    reference: str
    value: str
    quantity: int
    digikey_price: Optional[Decimal]
    mouser_price: Optional[Decimal]
    manufacturer_pn: str
    digikey_pn: str
    mouser_pn: str
    best_price: Optional[Decimal] = None
    best_supplier: str = ""
    extended_cost: Decimal = field(default_factory=lambda: Decimal("0.00"))
    dnp: bool = False

    def to_dict(self):
        """Convert to JSON-serializable dict."""
        return {
            'reference': self.reference,
            'value': self.value,
            'quantity': self.quantity,
            'digikey_price': str(self.digikey_price) if self.digikey_price else None,
            'mouser_price': str(self.mouser_price) if self.mouser_price else None,
            'manufacturer_pn': self.manufacturer_pn,
            'best_price': str(self.best_price) if self.best_price else None,
            'best_supplier': self.best_supplier,
            'extended_cost': str(self.extended_cost),
        }


@dataclass
class BoardBOM:
    """Represents a complete BOM for a single PCB board."""
    filename: str
    short_name: str
    display_name: str
    category: str
    components: List[Component] = field(default_factory=list)
    subtotal_raw: Decimal = field(default_factory=lambda: Decimal("0.00"))
    subtotal_with_buffer: Decimal = field(default_factory=lambda: Decimal("0.00"))
    component_count: int = 0
    unique_parts: int = 0
    missing_prices: int = 0
    missing_price_refs: List[str] = field(default_factory=list)

    def to_dict(self):
        """Convert to JSON-serializable dict."""
        return {
            'filename': self.filename,
            'short_name': self.short_name,
            'display_name': self.display_name,
            'category': self.category,
            'component_count': self.component_count,
            'unique_parts': self.unique_parts,
            'subtotal_raw': str(self.subtotal_raw),
            'subtotal_with_buffer': str(self.subtotal_with_buffer),
            'missing_prices': self.missing_prices,
            'missing_price_refs': self.missing_price_refs,
            'components': [c.to_dict() for c in self.components],
        }


# =============================================================================
# Parsing Functions
# =============================================================================

def parse_price(value: str) -> Optional[Decimal]:
    """
    Parse a price string from CSV, handling various invalid formats.

    Args:
        value: Price string from CSV (e.g., "0.26", "N/A", "", "--")

    Returns:
        Decimal price or None if invalid/missing
    """
    if not value:
        return None

    value = value.strip()

    # Handle known invalid values
    if value.upper() in ('N/A', '--', '~', ''):
        return None

    try:
        # Remove any currency symbols, commas, or extra whitespace
        cleaned = value.replace('$', '').replace(',', '').strip()
        if not cleaned:
            return None
        price = Decimal(cleaned)
        # Sanity check - prices shouldn't be negative
        if price < 0:
            return None
        return price
    except (ValueError, InvalidOperation):
        return None


def get_best_price(dk_price: Optional[Decimal], mo_price: Optional[Decimal]) -> Tuple[Optional[Decimal], str]:
    """
    Return the lowest non-null price and its supplier name.

    Args:
        dk_price: DigiKey price (or None)
        mo_price: Mouser price (or None)

    Returns:
        Tuple of (best_price, supplier_name)
    """
    if dk_price is None and mo_price is None:
        return None, "MISSING"
    if dk_price is None:
        return mo_price, "Mouser"
    if mo_price is None:
        return dk_price, "DigiKey"
    if dk_price <= mo_price:
        return dk_price, "DigiKey"
    return mo_price, "Mouser"


def parse_bom_csv(filepath: Path) -> List[Component]:
    """
    Parse a single BOM CSV file and return list of components.

    Args:
        filepath: Path to CSV file

    Returns:
        List of Component objects
    """
    components = []

    with open(filepath, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)

        for row in reader:
            # Skip components marked DNP (Do Not Populate)
            dnp_value = row.get(COL_DNP, '').strip()
            if dnp_value:  # Any non-empty value means DNP
                continue

            # Parse quantity (default to 1 if missing)
            qty_str = row.get(COL_QUANTITY, '1').strip()
            try:
                quantity = int(qty_str) if qty_str else 1
            except ValueError:
                quantity = 1

            # Parse prices
            dk_price = parse_price(row.get(COL_DIGIKEY_PRICE, ''))
            mo_price = parse_price(row.get(COL_MOUSER_PRICE, ''))

            # Determine best price (lowest non-null)
            best_price, supplier = get_best_price(dk_price, mo_price)

            # Create component
            component = Component(
                reference=row.get(COL_REFERENCE, '').strip(),
                value=row.get(COL_VALUE, '').strip(),
                quantity=quantity,
                digikey_price=dk_price,
                mouser_price=mo_price,
                manufacturer_pn=row.get(COL_MPN, '').strip(),
                digikey_pn=row.get(COL_DIGIKEY_PN, '').strip(),
                mouser_pn=row.get(COL_MOUSER_PN, '').strip(),
                best_price=best_price,
                best_supplier=supplier,
            )

            # Calculate extended cost
            if best_price is not None:
                component.extended_cost = (best_price * Decimal(quantity)).quantize(
                    Decimal("0.01"), rounding=ROUND_HALF_UP
                )

            components.append(component)

    return components


def process_board(filepath: Path, short_name: str, display_name: str, category: str,
                   price_db: Optional[Dict[str, Tuple[Decimal, str]]] = None) -> BoardBOM:
    """
    Process a single board's BOM file.

    Args:
        filepath: Path to CSV file
        short_name: Short name for the board
        display_name: Display name for tables
        category: Category (Power Management, Battery Management, Control/Interface)
        price_db: Optional cross-BOM price database for filling missing prices

    Returns:
        BoardBOM object with calculated totals
    """
    board = BoardBOM(
        filename=filepath.name,
        short_name=short_name,
        display_name=display_name,
        category=category,
    )

    components = parse_bom_csv(filepath)
    board.components = components
    board.unique_parts = len(components)
    board.component_count = sum(c.quantity for c in components)

    # Track cross-BOM price fills
    cross_bom_fills = []

    # Calculate totals and track missing prices
    for c in components:
        if c.best_price is None:
            # Try cross-BOM lookup
            if price_db and c.value in price_db:
                lookup_price, source_file = price_db[c.value]
                c.best_price = lookup_price
                c.best_supplier = f"Cross-BOM ({source_file})"
                c.extended_cost = (lookup_price * Decimal(c.quantity)).quantize(
                    Decimal("0.01"), rounding=ROUND_HALF_UP
                )
                cross_bom_fills.append(f"{c.reference}: {c.value} -> ${lookup_price} from {source_file}")
                board.subtotal_raw += c.extended_cost
            else:
                board.missing_prices += 1
                board.missing_price_refs.append(f"{c.reference}: {c.value}")
        else:
            board.subtotal_raw += c.extended_cost

    # Log cross-BOM fills if any
    if cross_bom_fills:
        print(f"    Cross-BOM price fills for {short_name}: {len(cross_bom_fills)}")

    # Apply 1.2x buffer
    board.subtotal_with_buffer = (board.subtotal_raw * SPARE_PARTS_MULTIPLIER).quantize(
        Decimal("0.01"), rounding=ROUND_HALF_UP
    )

    return board


# =============================================================================
# Cross-BOM Price Lookup
# =============================================================================

def build_price_database(bom_dir: Path) -> Dict[str, Tuple[Decimal, str]]:
    """
    Build a database of known prices by scanning all BOM files.

    This allows components with missing prices in one BOM to use prices
    from the same component in another BOM.

    Args:
        bom_dir: Directory containing BOM CSV files

    Returns:
        Dictionary mapping value (part identifier) to (best_price, source_file)
    """
    price_db = {}

    for filename in BOM_FILES.keys():
        filepath = bom_dir / filename
        if not filepath.exists():
            continue

        with open(filepath, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                # Skip DNP components
                if row.get(COL_DNP, '').strip():
                    continue

                value = row.get(COL_VALUE, '').strip()
                if not value:
                    continue

                dk_price = parse_price(row.get(COL_DIGIKEY_PRICE, ''))
                mo_price = parse_price(row.get(COL_MOUSER_PRICE, ''))
                best_price, supplier = get_best_price(dk_price, mo_price)

                if best_price is not None:
                    # Only store if we don't have a price yet, or if this is cheaper
                    if value not in price_db or best_price < price_db[value][0]:
                        price_db[value] = (best_price, filename)

    return price_db


# =============================================================================
# Main Processing
# =============================================================================

def calculate_all_boms(bom_dir: Path) -> Dict[str, BoardBOM]:
    """
    Process all BOM files and calculate totals.

    Args:
        bom_dir: Directory containing BOM CSV files

    Returns:
        Dictionary mapping filename to BoardBOM object
    """
    # First pass: build price database from all BOMs
    print("Building cross-BOM price database...")
    price_db = build_price_database(bom_dir)
    print(f"  Found prices for {len(price_db)} unique component values")

    results = {}

    for filename, (short_name, display_name, category) in BOM_FILES.items():
        filepath = bom_dir / filename
        if not filepath.exists():
            print(f"WARNING: Missing BOM file: {filename}")
            continue

        board = process_board(filepath, short_name, display_name, category, price_db)
        results[filename] = board

    return results


def calculate_totals(boards: Dict[str, BoardBOM]) -> Dict:
    """
    Calculate aggregate totals across all boards.

    Args:
        boards: Dictionary of BoardBOM objects

    Returns:
        Dictionary with totals
    """
    totals = {
        'raw_total': Decimal("0.00"),
        'buffered_total': Decimal("0.00"),
        'component_count': 0,
        'unique_parts': 0,
        'missing_prices': 0,
    }

    for board in boards.values():
        totals['raw_total'] += board.subtotal_raw
        totals['buffered_total'] += board.subtotal_with_buffer
        totals['component_count'] += board.component_count
        totals['unique_parts'] += board.unique_parts
        totals['missing_prices'] += board.missing_prices

    return totals


def calculate_category_totals(boards: Dict[str, BoardBOM]) -> Dict[str, Dict]:
    """
    Calculate totals by category.

    Args:
        boards: Dictionary of BoardBOM objects

    Returns:
        Dictionary mapping category to totals
    """
    categories = {}

    for board in boards.values():
        cat = board.category
        if cat not in categories:
            categories[cat] = {
                'raw_total': Decimal("0.00"),
                'buffered_total': Decimal("0.00"),
                'component_count': 0,
                'boards': [],
            }
        categories[cat]['raw_total'] += board.subtotal_raw
        categories[cat]['buffered_total'] += board.subtotal_with_buffer
        categories[cat]['component_count'] += board.component_count
        categories[cat]['boards'].append(board.short_name)

    return categories


# =============================================================================
# Output Generation
# =============================================================================

def generate_json_output(boards: Dict[str, BoardBOM], totals: Dict, categories: Dict) -> str:
    """Generate JSON output for verification and further processing."""
    output = {
        'generated': datetime.now().isoformat(),
        'multiplier': str(SPARE_PARTS_MULTIPLIER),
        'totals': {
            'raw_total': str(totals['raw_total']),
            'buffered_total': str(totals['buffered_total']),
            'component_count': totals['component_count'],
            'unique_parts': totals['unique_parts'],
            'missing_prices': totals['missing_prices'],
        },
        'categories': {
            cat: {
                'raw_total': str(vals['raw_total']),
                'buffered_total': str(vals['buffered_total']),
                'component_count': vals['component_count'],
                'boards': vals['boards'],
            }
            for cat, vals in categories.items()
        },
        'boards': {
            filename: board.to_dict()
            for filename, board in sorted(boards.items())
        },
    }

    return json.dumps(output, indent=2)


def generate_verification_log(boards: Dict[str, BoardBOM], totals: Dict, categories: Dict) -> str:
    """Generate human-readable verification log."""
    lines = []
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    lines.append("=" * 80)
    lines.append("BOM CALCULATION VERIFICATION LOG")
    lines.append(f"Generated: {timestamp}")
    lines.append(f"Multiplier: {SPARE_PARTS_MULTIPLIER}x (20% spare parts buffer)")
    lines.append("Pricing: Lowest of DigiKey and Mouser per component")
    lines.append("=" * 80)
    lines.append("")

    # Board-by-board details
    lines.append("-" * 80)
    lines.append("BOARD-BY-BOARD BREAKDOWN")
    lines.append("-" * 80)

    for filename, board in sorted(boards.items(), key=lambda x: x[1].category):
        lines.append("")
        lines.append(f"[{board.category}] {board.display_name}")
        lines.append(f"  File: {filename}")
        lines.append(f"  Unique Parts: {board.unique_parts}")
        lines.append(f"  Total Components: {board.component_count}")
        lines.append(f"  Raw Cost: ${board.subtotal_raw:.2f}")
        lines.append(f"  With 1.2x Buffer: ${board.subtotal_with_buffer:.2f}")

        if board.missing_prices > 0:
            lines.append(f"  WARNING: {board.missing_prices} components with missing prices:")
            for ref in board.missing_price_refs[:10]:  # Limit to first 10
                lines.append(f"    - {ref}")
            if len(board.missing_price_refs) > 10:
                lines.append(f"    ... and {len(board.missing_price_refs) - 10} more")

    # Category totals
    lines.append("")
    lines.append("-" * 80)
    lines.append("CATEGORY TOTALS")
    lines.append("-" * 80)

    for cat, vals in sorted(categories.items()):
        lines.append("")
        lines.append(f"{cat}:")
        lines.append(f"  Boards: {', '.join(vals['boards'])}")
        lines.append(f"  Components: {vals['component_count']}")
        lines.append(f"  Raw Cost: ${vals['raw_total']:.2f}")
        lines.append(f"  With Buffer: ${vals['buffered_total']:.2f}")

    # Grand totals
    lines.append("")
    lines.append("=" * 80)
    lines.append("GRAND TOTALS (ELECTRONICS)")
    lines.append("=" * 80)
    lines.append(f"Total Unique Parts: {totals['unique_parts']}")
    lines.append(f"Total Components: {totals['component_count']}")
    lines.append(f"Raw Cost: ${totals['raw_total']:.2f}")
    lines.append(f"With 1.2x Buffer: ${totals['buffered_total']:.2f}")
    lines.append(f"Components with Missing Prices: {totals['missing_prices']}")

    # Final summary with mechanical
    lines.append("")
    lines.append("=" * 80)
    lines.append("TOTAL PROJECT MATERIALS")
    lines.append("=" * 80)
    mechanical = Decimal("377.25")
    total_materials = totals['buffered_total'] + mechanical
    lines.append(f"Electronics (with buffer): ${totals['buffered_total']:.2f}")
    lines.append(f"Mechanical Components: ${mechanical:.2f}")
    lines.append(f"TOTAL MATERIALS: ${total_materials:.2f}")

    return "\n".join(lines)


def generate_latex_summary(boards: Dict[str, BoardBOM], totals: Dict) -> str:
    """Generate LaTeX table code for Section 13 BOM summary."""
    lines = []

    lines.append("% BOM Summary Table - Auto-generated by calculate_bom.py")
    lines.append("% Generated: " + datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
    lines.append("")

    # Group boards by category
    by_category = {}
    for filename, board in boards.items():
        cat = board.category
        if cat not in by_category:
            by_category[cat] = []
        by_category[cat].append(board)

    # Sort categories in desired order
    category_order = ['Power Management', 'Battery Management', 'Control/Interface']

    lines.append(r"\begin{longtable}{@{}p{0.36\textwidth}rrrp{0.08\textwidth}@{}}")
    lines.append(r"\caption{Bill of Materials -- PCB Summary (1.2$\times$ Buffer Applied)} \label{tab:bom} \\")
    lines.append(r"\toprule")
    lines.append(r"\textbf{Board} & \textbf{Parts} & \textbf{Raw Cost} & \textbf{Buffered} & \textbf{Ref} \\")
    lines.append(r"\midrule")
    lines.append(r"\endfirsthead")
    lines.append(r"\multicolumn{5}{c}{\tablename\ \thetable{} -- Continued from previous page} \\")
    lines.append(r"\toprule")
    lines.append(r"\textbf{Board} & \textbf{Parts} & \textbf{Raw Cost} & \textbf{Buffered} & \textbf{Ref} \\")
    lines.append(r"\midrule")
    lines.append(r"\endhead")
    lines.append(r"\midrule")
    lines.append(r"\multicolumn{5}{r}{Continued on next page} \\")
    lines.append(r"\endfoot")
    lines.append(r"\bottomrule")
    lines.append(r"\endlastfoot")
    lines.append("")

    # Label mapping
    label_map = {
        'STAR_AFE_BOM.csv': 'tab:bom-afe-full-app',
        'STAR_BUCK_BOOST_CHARGER_BOM.csv': 'tab:bom-charger-full-app',
        'STAR_FUEL_GAUGE_BOM.csv': 'tab:bom-fuel-full-app',
        'STAR_GATE_DRIVER_BOM.csv': 'tab:bom-gate-full-app',
        'STAR_LIDAR_BREAKOUT_BOM.csv': 'tab:bom-lidar-full-app',
        'STAR_MCU_BOM.csv': 'tab:bom-mcu-full-app',
        'STAR_MOTOR_DRIVER_BOM.csv': 'tab:bom-motor-full-app',
        'STAR_P3V3_BUCK_BOM.csv': 'tab:bom-p3v3-full-app',
        'STAR_P5V0_BUCK_BOM.csv': 'tab:bom-p5v0-full-app',
        'STAR_P6V0_BUCK_BOM.csv': 'tab:bom-p6v0-full-app',
        'STAR_USB-C_BOM.csv': 'tab:bom-usbc-full-app',
    }

    for cat in category_order:
        if cat not in by_category:
            continue
        lines.append(rf"\multicolumn{{5}}{{l}}{{\textbf{{{cat}}}}} \\")
        for board in sorted(by_category[cat], key=lambda b: b.display_name):
            label = label_map.get(board.filename, '')
            ref = rf"\ref{{{label}}}" if label else ''
            lines.append(
                f"{board.display_name} & {board.component_count} & "
                f"\\${board.subtotal_raw:.2f} & \\${board.subtotal_with_buffer:.2f} & {ref} \\\\"
            )
        lines.append(r"\addlinespace")
        lines.append("")

    # Electronics subtotal
    lines.append(r"\midrule")
    lines.append(
        f"\\textbf{{Electronics Subtotal}} & \\textbf{{{totals['component_count']}}} & "
        f"\\textbf{{\\${totals['raw_total']:.2f}}} & \\textbf{{\\${totals['buffered_total']:.2f}}} & \\\\"
    )
    lines.append(r"\addlinespace")

    # Mechanical
    mechanical = Decimal("377.25")
    lines.append(r"\multicolumn{5}{l}{\textbf{Mechanical Components}} \\")
    lines.append(f"Chassis, Motors, Hardware & -- & -- & \\$377.25 & \\ref{{tab:bom-mechanical}} \\\\")
    lines.append(r"\addlinespace")

    # Grand total
    total_materials = totals['buffered_total'] + mechanical
    lines.append(r"\midrule")
    lines.append(
        f"\\textbf{{Total Material Value}} & & & \\textbf{{\\${total_materials:.2f}}} & \\\\"
    )

    lines.append(r"\end{longtable}")

    return "\n".join(lines)


# =============================================================================
# Main Entry Point
# =============================================================================

def main():
    """Main entry point."""
    # Determine paths
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    bom_dir = project_root / 'figures' / 'bom'

    print("=" * 60)
    print("BOM Calculator for FTP Document")
    print("=" * 60)
    print(f"BOM Directory: {bom_dir}")
    print(f"Multiplier: {SPARE_PARTS_MULTIPLIER}x")
    print()

    # Check BOM directory exists
    if not bom_dir.exists():
        print(f"ERROR: BOM directory not found: {bom_dir}")
        sys.exit(1)

    # Process all BOMs
    print("Processing BOM files...")
    boards = calculate_all_boms(bom_dir)

    if not boards:
        print("ERROR: No BOM files processed!")
        sys.exit(1)

    print(f"Processed {len(boards)} board BOMs")

    # Calculate totals
    totals = calculate_totals(boards)
    categories = calculate_category_totals(boards)

    # Generate outputs
    print("\nGenerating outputs...")

    # JSON output
    json_path = bom_dir / 'bom_calculations.json'
    json_output = generate_json_output(boards, totals, categories)
    with open(json_path, 'w') as f:
        f.write(json_output)
    print(f"  Created: {json_path}")

    # Verification log
    log_path = bom_dir / 'bom_verification.log'
    log_output = generate_verification_log(boards, totals, categories)
    with open(log_path, 'w') as f:
        f.write(log_output)
    print(f"  Created: {log_path}")

    # LaTeX summary
    latex_path = bom_dir / 'bom_summary_table.tex'
    latex_output = generate_latex_summary(boards, totals)
    with open(latex_path, 'w') as f:
        f.write(latex_output)
    print(f"  Created: {latex_path}")

    # Print summary
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    print(f"Total Boards: {len(boards)}")
    print(f"Total Components: {totals['component_count']}")
    print(f"Components Missing Prices: {totals['missing_prices']}")
    print()
    print(f"Raw Electronics Cost: ${totals['raw_total']:.2f}")
    print(f"With 1.2x Buffer: ${totals['buffered_total']:.2f}")
    print(f"Mechanical Components: $377.25")
    mechanical = Decimal("377.25")
    total = totals['buffered_total'] + mechanical
    print(f"TOTAL MATERIALS: ${total:.2f}")
    print()

    if totals['missing_prices'] > 0:
        print(f"WARNING: {totals['missing_prices']} components have missing prices.")
        print("See bom_verification.log for details.")

    return 0


if __name__ == '__main__':
    sys.exit(main())
