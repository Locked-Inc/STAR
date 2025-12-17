#!/usr/bin/env python3
"""
FTP Comprehensive Math Verification Script

Verifies all totals, sums, and calculations throughout the FTP document.
Consolidates verification from verify_tasks.py, verify_dates.py, and verify_critical_path.py.

Usage: python3 scripts/verify_math.py
Exit code: Number of failures (0 = all pass)
"""

import re
import os
import csv
import yaml
from pathlib import Path
from datetime import datetime
from collections import defaultdict

# =============================================================================
# ANSI Color Codes for Terminal Output
# =============================================================================
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
CYAN = "\033[96m"
RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"

# =============================================================================
# Configuration
# =============================================================================
PHASE_NAMES = {
    '1.1': 'Research',
    '1.2': 'Design',
    '1.3': 'Build',
    '1.4': 'Test',
    '1.5': 'Close',
    '1.6': 'PM Overhead',
}

GANTT_FILES = {
    'Research': 'gantt_research.tex',
    'Design': 'gantt_design.tex',
    'Build': 'gantt_build.tex',
    'Test': 'gantt_test.tex',
    'Close': 'gantt_close.tex',
}

PHASE_ID_MAP = {
    'Research': '1.1',
    'Design': '1.2',
    'Build': '1.3',
    'Test': '1.4',
    'Close': '1.5',
}

# All phases including PM Overhead for total counts
ALL_PHASE_IDS = ['1.1', '1.2', '1.3', '1.4', '1.5', '1.6']

# =============================================================================
# Output Formatting
# =============================================================================
def print_header():
    """Print the main report header."""
    print()
    print(f"{CYAN}{BOLD}{'=' * 70}{RESET}")
    print(f"{CYAN}{BOLD}{'FTP COMPREHENSIVE MATH VERIFICATION REPORT':^70}{RESET}")
    print(f"{CYAN}{BOLD}{'=' * 70}{RESET}")
    print()


def print_section(number, title):
    """Print a section header."""
    print(f"\n{BOLD}{'-' * 70}{RESET}")
    print(f"{BOLD}{number}. {title}{RESET}")
    print(f"{BOLD}{'-' * 70}{RESET}")


def print_pass(message):
    """Print a pass result."""
    print(f"  {GREEN}[PASS]{RESET} {message}")


def print_fail(message):
    """Print a fail result."""
    print(f"  {RED}[FAIL]{RESET} {message}")


def print_warn(message):
    """Print a warning result."""
    print(f"  {YELLOW}[WARN]{RESET} {message}")


def print_info(message):
    """Print info message."""
    print(f"  {DIM}[INFO]{RESET} {message}")


def print_summary(passed, failed, warnings):
    """Print the final summary."""
    total = passed + failed
    print()
    print(f"{CYAN}{BOLD}{'=' * 70}{RESET}")
    print(f"{CYAN}{BOLD}{'SUMMARY':^70}{RESET}")
    print(f"{CYAN}{BOLD}{'=' * 70}{RESET}")
    print(f"  Total Checks:  {total}")
    print(f"  {GREEN}Passed:        {passed}{RESET}")
    print(f"  {RED}Failed:        {failed}{RESET}")
    print(f"  {YELLOW}Warnings:      {warnings}{RESET}")
    print()

    if failed == 0:
        print(f"  {GREEN}{BOLD}All verifications passed!{RESET}")
    else:
        print(f"  {RED}{BOLD}ATTENTION: {failed} verification(s) failed!{RESET}")

    print(f"{CYAN}{BOLD}{'=' * 70}{RESET}")
    print()


# =============================================================================
# Data Loading Functions
# =============================================================================
def load_activities(filepath: str) -> dict:
    """Load activities from YAML file."""
    with open(filepath, 'r') as f:
        return yaml.safe_load(f)


def read_file(filepath: str) -> str:
    """Read entire file contents."""
    with open(filepath, 'r') as f:
        return f.read()


# =============================================================================
# Task Count Verification
# =============================================================================
def count_leaf_tasks(activities: dict) -> tuple:
    """Count tasks that have duration_days (leaf nodes) by phase."""
    counts = defaultdict(int)
    task_details = defaultdict(list)

    for task_id, task in activities.items():
        if isinstance(task, dict) and 'duration_days' in task:
            task_id_str = str(task_id)
            parts = task_id_str.split('.')
            if len(parts) >= 2:
                phase = f"{parts[0]}.{parts[1]}"
                counts[phase] += 1
                task_details[phase].append({
                    'id': task_id,
                    'name': task.get('name', 'Unknown'),
                    'duration': task.get('duration_days', 0)
                })

    return dict(counts), dict(task_details)


def count_dependencies(activities: dict) -> int:
    """Count total dependencies in activities."""
    total = 0
    for task_id, task in activities.items():
        if isinstance(task, dict) and 'depends_on' in task:
            deps = task['depends_on']
            if isinstance(deps, list):
                total += len(deps)
    return total


def parse_gantt_task_count(filepath: str) -> int:
    """Parse claimed task count from Gantt file header."""
    content = read_file(filepath)
    match = re.search(r'Total Tasks:\}\s*(\d+)', content)
    return int(match.group(1)) if match else None


def parse_gantt_bar_count(filepath: str) -> int:
    """Count actual \\ganttbar entries in Gantt file."""
    content = read_file(filepath)
    # Count \ganttbar entries (not \ganttgroup)
    bars = re.findall(r'\\ganttbar\[', content)
    return len(bars)


def parse_overview_activity_count(filepath: str) -> int:
    """Parse claimed activity count from overview text."""
    content = read_file(filepath)
    match = re.search(r'showing all (\d+) activities', content)
    if match:
        return int(match.group(1))
    # Also try "all X activities"
    match = re.search(r'all (\d+) activities', content)
    return int(match.group(1)) if match else None


def parse_dependencies_claim(filepath: str) -> int:
    """Parse claimed dependencies count."""
    content = read_file(filepath)
    match = re.search(r'All (\d+) dependencies', content)
    return int(match.group(1)) if match else None


def verify_task_counts(project_root: Path) -> tuple:
    """Verify task counts across all sources."""
    passed = 0
    failed = 0
    warnings = 0

    print_section(1, "TASK COUNTS")

    # Load YAML
    activities_file = project_root / 'FTP' / 'data' / 'activities_cache.yaml'
    if not activities_file.exists():
        print_fail(f"activities_cache.yaml not found")
        return 0, 1, 0

    activities = load_activities(activities_file)
    leaf_counts, task_details = count_leaf_tasks(activities)

    # Calculate total (include all phases including PM Overhead)
    total_yaml = sum(leaf_counts.get(phase_id, 0) for phase_id in ALL_PHASE_IDS)

    # Check each Gantt file
    gantt_dir = project_root / 'gantt'
    print(f"\n  {'Phase':<12} {'YAML':>6} {'Gantt':>6} {'Bars':>6}  Status")
    print(f"  {'-'*12} {'-'*6} {'-'*6} {'-'*6}  {'-'*10}")

    total_gantt_claimed = 0
    total_gantt_actual = 0

    for phase_name, phase_id in PHASE_ID_MAP.items():
        yaml_count = leaf_counts.get(phase_id, 0)
        gantt_file = gantt_dir / GANTT_FILES.get(phase_name, '')

        if gantt_file.exists():
            gantt_claimed = parse_gantt_task_count(gantt_file)
            gantt_actual = parse_gantt_bar_count(gantt_file)
            total_gantt_claimed += gantt_claimed or 0
            total_gantt_actual += gantt_actual

            # Special case: Close phase has 1.5.3 omitted intentionally (runs throughout project)
            if phase_name == 'Close' and yaml_count == gantt_claimed and gantt_actual == yaml_count - 1:
                status = f"{GREEN}MATCH{RESET} (1.5.3 omitted per note)"
                passed += 1
            elif yaml_count == gantt_claimed == gantt_actual:
                status = f"{GREEN}MATCH{RESET}"
                passed += 1
            elif yaml_count == gantt_actual:
                status = f"{YELLOW}CLAIM MISMATCH{RESET}"
                warnings += 1
            else:
                status = f"{RED}MISMATCH{RESET}"
                failed += 1

            print(f"  {phase_name:<12} {yaml_count:>6} {gantt_claimed or 'N/A':>6} {gantt_actual:>6}  {status}")
        else:
            print(f"  {phase_name:<12} {yaml_count:>6} {'N/A':>6} {'N/A':>6}  {YELLOW}NO FILE{RESET}")
            warnings += 1

    # PM Overhead (no Gantt file)
    pm_count = leaf_counts.get('1.6', 0)
    print(f"  {'PM Overhead':<12} {pm_count:>6} {'N/A':>6} {'N/A':>6}  {DIM}(no Gantt){RESET}")

    print(f"  {'-'*12} {'-'*6} {'-'*6} {'-'*6}")
    print(f"  {'TOTAL':<12} {total_yaml:>6} {total_gantt_claimed + pm_count:>6} {total_gantt_actual + pm_count:>6}")

    # Check overview claim in 08_gantt.tex
    gantt_tex = project_root / 'sections' / '08_gantt.tex'
    if gantt_tex.exists():
        overview_count = parse_overview_activity_count(gantt_tex)
        if overview_count:
            if overview_count == total_yaml:
                print_pass(f"Section 08 claims {overview_count} activities = YAML count ({total_yaml})")
                passed += 1
            else:
                print_fail(f"Section 08 claims {overview_count} activities, YAML has {total_yaml}")
                failed += 1

    # Check dependencies
    actual_deps = count_dependencies(activities)
    claimed_deps = parse_dependencies_claim(gantt_tex)
    if claimed_deps:
        if claimed_deps == actual_deps:
            print_pass(f"Dependencies: claimed {claimed_deps} = actual {actual_deps}")
            passed += 1
        else:
            print_fail(f"Dependencies: claimed {claimed_deps}, actual {actual_deps}")
            failed += 1

    return passed, failed, warnings


# =============================================================================
# Phase Duration Verification
# =============================================================================
def parse_date(date_str: str, base_year: int = 2025) -> datetime:
    """Parse a date string with various formats."""
    import warnings
    date_str = date_str.strip()

    # Try formats with year first
    formats_with_year = [
        "%B %d, %Y",      # August 26, 2025
        "%b %d, %Y",      # Aug 26, 2025
        "%B %d %Y",       # August 26 2025
        "%b %d %Y",       # Aug 26 2025
    ]

    for fmt in formats_with_year:
        try:
            return datetime.strptime(date_str, fmt)
        except ValueError:
            continue

    # Try formats without year (with warning suppression)
    formats_no_year = [
        "%B %d",          # August 26
        "%b %d",          # Aug 26
    ]

    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        for fmt in formats_no_year:
            try:
                parsed = datetime.strptime(date_str, fmt)
                # Infer year based on month
                if parsed.month >= 8:  # Aug-Dec is 2025
                    parsed = parsed.replace(year=2025)
                else:  # Jan-May is 2026
                    parsed = parsed.replace(year=2026)
                return parsed
            except ValueError:
                continue

    return None


def calculate_days(start_date: datetime, end_date: datetime) -> int:
    """Calculate calendar days between dates (inclusive)."""
    return (end_date - start_date).days + 1


def parse_schedule_table(filepath: str) -> list:
    """Parse phase durations from schedule table."""
    content = read_file(filepath)
    phases = []

    # Pattern for table rows: Phase & Duration & Dates
    # Example: Research & 67 days & Aug 26 -- Oct 31, 2025 \\
    pattern = r'(\w+)\s*&\s*(\d+)\s*days?\s*&\s*([^\\]+)\s*\\\\'

    for match in re.finditer(pattern, content):
        phase = match.group(1).strip()
        claimed_days = int(match.group(2))
        date_range = match.group(3).strip()

        # Parse date range (format: "Aug 26 -- Oct 31, 2025" or "Aug 26, 2025 -- Oct 31, 2025")
        if '--' in date_range:
            parts = date_range.split('--')
            start_str = parts[0].strip()
            end_str = parts[1].strip()

            start_date = parse_date(start_str)
            end_date = parse_date(end_str)

            phases.append({
                'phase': phase,
                'claimed_days': claimed_days,
                'start_date': start_date,
                'end_date': end_date,
            })

    return phases


def parse_gantt_duration(filepath: str) -> tuple:
    """Parse duration and dates from Gantt file header."""
    content = read_file(filepath)

    # Pattern: Phase Duration:} 67 days (Aug 26, 2025 -- Oct 31, 2025)
    pattern = r'Phase Duration:\}\s*(\d+)\s*days?\s*\(([^)]+)\)'
    match = re.search(pattern, content)

    if match:
        claimed_days = int(match.group(1))
        date_range = match.group(2)

        if '--' in date_range:
            parts = date_range.split('--')
            start_date = parse_date(parts[0].strip())
            end_date = parse_date(parts[1].strip())
            return claimed_days, start_date, end_date

    return None, None, None


def verify_phase_durations(project_root: Path) -> tuple:
    """Verify phase duration calculations."""
    passed = 0
    failed = 0
    warnings = 0

    print_section(2, "PHASE DURATIONS")

    gantt_tex = project_root / 'sections' / '08_gantt.tex'
    if not gantt_tex.exists():
        print_fail("08_gantt.tex not found")
        return 0, 1, 0

    phases = parse_schedule_table(gantt_tex)

    print(f"\n  {'Phase':<12} {'Claimed':>10} {'Calculated':>12}  Status")
    print(f"  {'-'*12} {'-'*10} {'-'*12}  {'-'*10}")

    for phase in phases:
        if phase['phase'] in ['Total', 'Project']:
            continue

        claimed = phase['claimed_days']
        start = phase['start_date']
        end = phase['end_date']

        if start and end:
            calculated = calculate_days(start, end)

            if claimed == calculated:
                status = f"{GREEN}MATCH{RESET}"
                passed += 1
            else:
                status = f"{RED}MISMATCH{RESET}"
                failed += 1

            print(f"  {phase['phase']:<12} {claimed:>7} days {calculated:>9} days  {status}")
        else:
            print(f"  {phase['phase']:<12} {claimed:>7} days {'ERROR':>9}      {YELLOW}PARSE ERROR{RESET}")
            warnings += 1

    # Cross-check with Gantt file headers
    print(f"\n  {BOLD}Gantt File Header Cross-Check:{RESET}")
    gantt_dir = project_root / 'gantt'

    for phase_name in ['Research', 'Design', 'Build', 'Test', 'Close']:
        gantt_file = gantt_dir / GANTT_FILES[phase_name]
        if gantt_file.exists():
            claimed, start, end = parse_gantt_duration(gantt_file)
            if claimed and start and end:
                calculated = calculate_days(start, end)
                if claimed == calculated:
                    print_pass(f"{GANTT_FILES[phase_name]}: {claimed} days matches date range")
                    passed += 1
                else:
                    print_fail(f"{GANTT_FILES[phase_name]}: claims {claimed} days, dates give {calculated}")
                    failed += 1

    return passed, failed, warnings


# =============================================================================
# Critical Path Verification
# =============================================================================
def count_critical_path_tasks(filepath: str) -> tuple:
    """Count tasks marked with red (critical path) in Gantt file."""
    content = read_file(filepath)

    # Pattern for critical path tasks: bar/.append style={fill=red!50}
    critical_tasks = re.findall(r'\\ganttbar\[.*?fill=red.*?\]\{([^}]+)\}', content)

    # Extract task IDs
    task_ids = []
    for task in critical_tasks:
        match = re.match(r'([\d.]+):', task)
        if match:
            task_ids.append(match.group(1))

    return len(critical_tasks), task_ids


def parse_cp_claim_in_note(filepath: str) -> int:
    """Parse claimed CP count from note in Gantt file."""
    content = read_file(filepath)
    # Pattern: "critical path (X tasks" or "X tasks in this phase"
    match = re.search(r'\((\d+) tasks in this phase\)', content)
    return int(match.group(1)) if match else None


def parse_critical_path_metrics(filepath: str) -> dict:
    """Parse critical path metrics from section file."""
    content = read_file(filepath)
    metrics = {}

    patterns = {
        'total_cp': r'Total Activities on Critical Path\s*&\s*(\d+)',
        'cp_duration': r'Critical Path Duration\s*&\s*(\d+)\s*days',
        'total_activities': r'Total Project Activities\s*&\s*(\d+)',
        'percentage': r'Percentage on Critical Path\s*&\s*(\d+)%',
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            metrics[key] = int(match.group(1))

    return metrics


def verify_critical_path(project_root: Path) -> tuple:
    """Verify critical path metrics."""
    passed = 0
    failed = 0
    warnings = 0

    print_section(3, "CRITICAL PATH METRICS")

    # Load activities for duration calculation
    activities_file = project_root / 'FTP' / 'data' / 'activities_cache.yaml'
    activities = load_activities(activities_file) if activities_file.exists() else {}

    # Count CP tasks from Gantt files
    gantt_dir = project_root / 'gantt'
    total_cp_tasks = 0
    total_cp_duration = 0

    print(f"\n  {'Phase':<12} {'Actual':>8} {'Claimed':>8}  Status")
    print(f"  {'-'*12} {'-'*8} {'-'*8}  {'-'*10}")

    for phase_name in ['Research', 'Design', 'Build', 'Test', 'Close']:
        gantt_file = gantt_dir / GANTT_FILES[phase_name]
        if gantt_file.exists():
            actual, task_ids = count_critical_path_tasks(gantt_file)
            claimed = parse_cp_claim_in_note(gantt_file)
            total_cp_tasks += actual

            # Calculate duration
            for tid in task_ids:
                if tid in activities and 'duration_days' in activities[tid]:
                    total_cp_duration += activities[tid]['duration_days']

            if claimed is None:
                status = f"{YELLOW}NO CLAIM{RESET}"
                warnings += 1
            elif actual == claimed:
                status = f"{GREEN}MATCH{RESET}"
                passed += 1
            else:
                status = f"{RED}MISMATCH{RESET}"
                failed += 1

            print(f"  {phase_name:<12} {actual:>8} {claimed or 'N/A':>8}  {status}")

    print(f"  {'-'*12} {'-'*8}")
    print(f"  {'TOTAL':<12} {total_cp_tasks:>8}")

    # Check section metrics
    cp_tex = project_root / 'sections' / '10_critical_path.tex'
    if cp_tex.exists():
        metrics = parse_critical_path_metrics(cp_tex)

        print(f"\n  {BOLD}Section 10 Metrics:{RESET}")

        # CP count
        if 'total_cp' in metrics:
            if metrics['total_cp'] == total_cp_tasks:
                print_pass(f"CP count: section claims {metrics['total_cp']} = actual {total_cp_tasks}")
                passed += 1
            else:
                print_fail(f"CP count: section claims {metrics['total_cp']}, actual {total_cp_tasks}")
                failed += 1

        # Total activities (include PM Overhead)
        leaf_counts, _ = count_leaf_tasks(activities)
        total_activities = sum(leaf_counts.get(pid, 0) for pid in ALL_PHASE_IDS)

        if 'total_activities' in metrics:
            if metrics['total_activities'] == total_activities:
                print_pass(f"Total activities: section claims {metrics['total_activities']} = actual {total_activities}")
                passed += 1
            else:
                print_fail(f"Total activities: section claims {metrics['total_activities']}, actual {total_activities}")
                failed += 1

        # Percentage
        if total_activities > 0:
            calculated_pct = round((total_cp_tasks / total_activities) * 100)
            if 'percentage' in metrics:
                if metrics['percentage'] == calculated_pct:
                    print_pass(f"Percentage: section claims {metrics['percentage']}% = calculated {calculated_pct}%")
                    passed += 1
                else:
                    print_fail(f"Percentage: section claims {metrics['percentage']}%, calculated {calculated_pct}%")
                    failed += 1

        # CP Duration
        if 'cp_duration' in metrics:
            print_info(f"CP Duration: section claims {metrics['cp_duration']} days, sum of task durations = {total_cp_duration} days")
            print_info("Note: Duration based on schedule, not sum of durations")

    return passed, failed, warnings


# =============================================================================
# BOM Verification
# =============================================================================
def verify_bom_counts(project_root: Path) -> tuple:
    """Verify BOM component counts and calculated costs."""
    import json
    passed = 0
    failed = 0
    warnings = 0

    print_section(4, "BOM COMPONENT COUNTS AND COSTS")

    bom_dir = project_root / 'figures' / 'bom'
    if not bom_dir.exists():
        print_warn("BOM directory not found")
        return 0, 0, 1

    # Check for calculations JSON
    json_file = bom_dir / 'bom_calculations.json'
    if json_file.exists():
        print_info(f"Found BOM calculations: {json_file.name}")
        with open(json_file, 'r') as f:
            calc_data = json.load(f)

        # Display calculated totals
        totals = calc_data.get('totals', {})
        print(f"\n  {BOLD}Calculated BOM Totals:{RESET}")
        print(f"  Total Components: {totals.get('component_count', 'N/A')}")
        print(f"  Raw Cost: ${totals.get('raw_total', 'N/A')}")
        print(f"  With 1.2x Buffer: ${totals.get('buffered_total', 'N/A')}")
        print(f"  Missing Prices: {totals.get('missing_prices', 'N/A')}")

        # Verify each board
        print(f"\n  {'Board':<30} {'Parts':>7} {'Raw Cost':>12} {'Buffered':>12}  Status")
        print(f"  {'-'*30} {'-'*7} {'-'*12} {'-'*12}  {'-'*8}")

        boards = calc_data.get('boards', {})
        for filename, board in sorted(boards.items()):
            parts = board.get('component_count', 0)
            raw = board.get('subtotal_raw', '0.00')
            buffered = board.get('subtotal_with_buffer', '0.00')
            missing = board.get('missing_prices', 0)

            if missing > 0:
                status = f"{YELLOW}{missing} missing{RESET}"
                warnings += 1
            else:
                status = f"{GREEN}OK{RESET}"
                passed += 1

            short_name = board.get('short_name', filename[:25])
            print(f"  {short_name:<30} {parts:>7} ${raw:>11} ${buffered:>11}  {status}")

        print(f"  {'-'*30} {'-'*7} {'-'*12} {'-'*12}")
        print(f"  {'TOTAL':<30} {totals.get('component_count', 0):>7} ${totals.get('raw_total', '0'):>11} ${totals.get('buffered_total', '0'):>11}")

        # Verify 1.2x multiplier
        try:
            raw = float(totals.get('raw_total', 0))
            buffered = float(totals.get('buffered_total', 0))
            expected_buffered = raw * 1.2
            if abs(buffered - expected_buffered) < 0.05:
                print_pass(f"1.2x buffer correctly applied: ${raw:.2f} * 1.2 = ${expected_buffered:.2f}")
                passed += 1
            else:
                print_fail(f"1.2x buffer mismatch: ${raw:.2f} * 1.2 = ${expected_buffered:.2f}, got ${buffered:.2f}")
                failed += 1
        except (ValueError, TypeError):
            print_warn("Could not verify 1.2x multiplier")
            warnings += 1

    else:
        print_warn("bom_calculations.json not found - run calculate_bom.py first")
        warnings += 1

        # Fall back to CSV count
        bom_files = list(bom_dir.glob('STAR_*.csv'))
        if bom_files:
            print_info(f"Found {len(bom_files)} BOM CSV files")
            for bom_file in sorted(bom_files):
                try:
                    with open(bom_file, 'r') as f:
                        reader = csv.DictReader(f)
                        rows = list(reader)
                    print_info(f"  {bom_file.name}: {len(rows)} components")
                except Exception:
                    pass

    return passed, failed, warnings


# =============================================================================
# Budget Verification
# =============================================================================
def verify_budget_math(project_root: Path) -> tuple:
    """Verify budget calculations and cross-reference with BOM data."""
    import json
    passed = 0
    failed = 0
    warnings = 0

    print_section(5, "BUDGET CALCULATIONS")

    budget_tex = project_root / 'sections' / '13_budget.tex'
    if not budget_tex.exists():
        print_warn("13_budget.tex not found")
        return 0, 0, 1

    content = read_file(budget_tex)

    # Load BOM calculations for cross-reference
    bom_json = project_root / 'figures' / 'bom' / 'bom_calculations.json'
    bom_data = None
    if bom_json.exists():
        with open(bom_json, 'r') as f:
            bom_data = json.load(f)

    # Extract dollar amounts from budget summary table
    amounts = re.findall(r'&\s*\\\$([0-9,.]+)', content)

    if amounts:
        values = []
        for amt in amounts:
            try:
                val = float(amt.replace(',', ''))
                values.append(val)
            except:
                pass

        if all(v == 0.0 for v in values):
            print_warn("All budget values are placeholders ($0.00)")
            warnings += 1
        else:
            print_info("Budget values found - checking calculations")

            # Look for key values in budget
            electronics_raw_match = re.search(r'Electronics \(Raw Cost\)\s*&\s*\\\$([0-9,.]+)', content)
            electronics_buffered_match = re.search(r'Electronics.*1\.2.*Buffer.*&\s*\\\$([0-9,.]+)', content)
            mechanical_match = re.search(r'Mechanical Components\s*&\s*\\\$([0-9,.]+)', content)
            total_material_match = re.search(r'Total Material Value.*&\s*\\\$([0-9,.]+)', content)

            if electronics_raw_match and bom_data:
                latex_raw = float(electronics_raw_match.group(1).replace(',', ''))
                bom_raw = float(bom_data['totals']['raw_total'])
                if abs(latex_raw - bom_raw) < 0.05:
                    print_pass(f"Electronics raw cost matches BOM: ${latex_raw:.2f}")
                    passed += 1
                else:
                    print_fail(f"Electronics raw cost mismatch: LaTeX=${latex_raw:.2f}, BOM=${bom_raw:.2f}")
                    failed += 1

            if electronics_buffered_match and bom_data:
                latex_buffered = float(electronics_buffered_match.group(1).replace(',', ''))
                bom_buffered = float(bom_data['totals']['buffered_total'])
                if abs(latex_buffered - bom_buffered) < 0.05:
                    print_pass(f"Electronics buffered cost matches BOM: ${latex_buffered:.2f}")
                    passed += 1
                else:
                    print_fail(f"Electronics buffered mismatch: LaTeX=${latex_buffered:.2f}, BOM=${bom_buffered:.2f}")
                    failed += 1

            if mechanical_match and total_material_match and electronics_buffered_match:
                mechanical = float(mechanical_match.group(1).replace(',', ''))
                total = float(total_material_match.group(1).replace(',', ''))
                buffered = float(electronics_buffered_match.group(1).replace(',', ''))
                expected_total = mechanical + buffered
                if abs(total - expected_total) < 0.05:
                    print_pass(f"Total materials correct: ${buffered:.2f} + ${mechanical:.2f} = ${total:.2f}")
                    passed += 1
                else:
                    print_fail(f"Total materials mismatch: ${buffered:.2f} + ${mechanical:.2f} = ${expected_total:.2f}, got ${total:.2f}")
                    failed += 1

            # Check reserve calculation
            reserve_match = re.search(r'Reserve \((\d+)%\)\s*&\s*\\\$([0-9,.]+)', content)
            if reserve_match:
                reserve_pct = int(reserve_match.group(1))
                reserve_val = float(reserve_match.group(2).replace(',', ''))
                print_info(f"Reserve: {reserve_pct}% = ${reserve_val:.2f}")
    else:
        print_warn("Could not parse budget amounts")
        warnings += 1

    # Check labor costs pattern
    labor_match = re.search(r'Labor Costs \((\w+) hours @ \$(\d+)/hr\)', content)
    if labor_match:
        hours = labor_match.group(1)
        rate = labor_match.group(2)
        if hours == 'XXX':
            print_warn("Labor hours is placeholder (XXX)")
            warnings += 1
        else:
            print_info(f"Labor: {hours} hours @ ${rate}/hr")

    return passed, failed, warnings


# =============================================================================
# Cross-Reference Verification
# =============================================================================
def verify_cross_references(project_root: Path) -> tuple:
    """Verify consistency across multiple sources."""
    passed = 0
    failed = 0
    warnings = 0

    print_section(6, "CROSS-REFERENCE CONSISTENCY")

    # Load all sources
    activities_file = project_root / 'FTP' / 'data' / 'activities_cache.yaml'
    gantt_tex = project_root / 'sections' / '08_gantt.tex'
    cp_tex = project_root / 'sections' / '10_critical_path.tex'

    sources = {}

    # YAML count (include PM Overhead)
    if activities_file.exists():
        activities = load_activities(activities_file)
        leaf_counts, _ = count_leaf_tasks(activities)
        sources['YAML'] = sum(leaf_counts.get(pid, 0) for pid in ALL_PHASE_IDS)

    # Section 08 count
    if gantt_tex.exists():
        sources['Section 08'] = parse_overview_activity_count(gantt_tex)

    # Section 10 count
    if cp_tex.exists():
        metrics = parse_critical_path_metrics(cp_tex)
        sources['Section 10'] = metrics.get('total_activities')

    # Compare all sources
    print(f"\n  {BOLD}Total Activity Count Across Sources:{RESET}")
    for source, count in sources.items():
        print(f"    {source}: {count}")

    unique_values = set(v for v in sources.values() if v is not None)
    if len(unique_values) == 1:
        print_pass(f"All sources agree: {unique_values.pop()} activities")
        passed += 1
    elif len(unique_values) > 1:
        print_fail(f"Sources disagree: {unique_values}")
        failed += 1

    return passed, failed, warnings


# =============================================================================
# Main Function
# =============================================================================
def main():
    # Determine project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    print_header()

    total_passed = 0
    total_failed = 0
    total_warnings = 0

    # Run all verifications
    p, f, w = verify_task_counts(project_root)
    total_passed += p
    total_failed += f
    total_warnings += w

    p, f, w = verify_phase_durations(project_root)
    total_passed += p
    total_failed += f
    total_warnings += w

    p, f, w = verify_critical_path(project_root)
    total_passed += p
    total_failed += f
    total_warnings += w

    p, f, w = verify_bom_counts(project_root)
    total_passed += p
    total_failed += f
    total_warnings += w

    p, f, w = verify_budget_math(project_root)
    total_passed += p
    total_failed += f
    total_warnings += w

    p, f, w = verify_cross_references(project_root)
    total_passed += p
    total_failed += f
    total_warnings += w

    print_summary(total_passed, total_failed, total_warnings)

    return total_failed


if __name__ == '__main__':
    exit(main())
