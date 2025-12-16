#!/usr/bin/env python3
"""
FTP Date/Duration Verification Script

Verifies that all phase durations match their date ranges throughout the FTP document.
"""

import re
import os
from datetime import datetime
from pathlib import Path

# ANSI colors for terminal output
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
RESET = "\033[0m"
BOLD = "\033[1m"


def days_between(start_date: datetime, end_date: datetime) -> int:
    """Calculate calendar days between two dates (inclusive)."""
    return (end_date - start_date).days + 1


def parse_date(date_str: str, default_year: int = None) -> datetime:
    """Parse various date formats into datetime object."""
    date_str = date_str.strip()

    # Try different formats
    formats = [
        "%B %d, %Y",      # August 26, 2025
        "%b %d, %Y",      # Aug 26, 2025
        "%B %d %Y",       # August 26 2025
        "%b %d %Y",       # Aug 26 2025
    ]

    for fmt in formats:
        try:
            return datetime.strptime(date_str, fmt)
        except ValueError:
            continue

    # Try without year (e.g., "Aug 26", "Oct 31")
    formats_no_year = [
        "%B %d",  # August 26
        "%b %d",  # Aug 26
    ]

    for fmt in formats_no_year:
        try:
            dt = datetime.strptime(date_str, fmt)
            if default_year:
                dt = dt.replace(year=default_year)
            return dt
        except ValueError:
            continue

    raise ValueError(f"Could not parse date: {date_str}")


def parse_schedule_table(filepath: str) -> list:
    """Parse the schedule summary table from 08_gantt.tex."""
    entries = []

    with open(filepath, 'r') as f:
        content = f.read()

    # Pattern for table rows: Phase & X days & Date1 -- Date2, Year \\
    # Example: \rowcolor{researchblue!30} Research & 55 days & Aug 26 -- Oct 31, 2025 \\
    pattern = r'\\rowcolor\{[^}]+\}\s*(\w+)\s*&\s*(\d+)\s*days\s*&\s*([^\\]+)\s*\\\\'

    for match in re.finditer(pattern, content):
        phase = match.group(1)
        claimed_days = int(match.group(2))
        date_range = match.group(3).strip()

        # Parse date range (e.g., "Aug 26 -- Oct 31, 2025" or "Nov 1, 2025 -- Feb 28, 2026")
        dates = re.split(r'\s*--\s*', date_range)
        if len(dates) == 2:
            entries.append({
                'source': 'Schedule Table',
                'phase': phase,
                'claimed_days': claimed_days,
                'start_str': dates[0].strip(),
                'end_str': dates[1].strip(),
                'raw': date_range
            })

    # Also get the total project line
    total_pattern = r'\\textbf\{Total Project\}\s*&\s*\\textbf\{(\d+)\s*days\}\s*&\s*\\textbf\{([^}]+)\}'
    total_match = re.search(total_pattern, content)
    if total_match:
        claimed_days = int(total_match.group(1))
        date_range = total_match.group(2).strip()
        dates = re.split(r'\s*--\s*', date_range)
        if len(dates) == 2:
            entries.append({
                'source': 'Schedule Table',
                'phase': 'Total Project',
                'claimed_days': claimed_days,
                'start_str': dates[0].strip(),
                'end_str': dates[1].strip(),
                'raw': date_range
            })

    return entries


def parse_gantt_headers(gantt_dir: str) -> list:
    """Parse phase duration from Gantt file headers."""
    entries = []

    gantt_files = [
        ('gantt_research.tex', 'Research'),
        ('gantt_design.tex', 'Design'),
        ('gantt_build.tex', 'Build'),
        ('gantt_test.tex', 'Test'),
        ('gantt_close.tex', 'Close'),
    ]

    # Pattern: Phase Duration: X days (Date1 -- Date2)
    pattern = r'Phase Duration:\}\s*(\d+)\s*days\s*\(([^)]+)\)'

    for filename, phase in gantt_files:
        filepath = os.path.join(gantt_dir, filename)
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                content = f.read()

            match = re.search(pattern, content)
            if match:
                claimed_days = int(match.group(1))
                date_range = match.group(2).strip()
                dates = re.split(r'\s*--\s*', date_range)
                if len(dates) == 2:
                    entries.append({
                        'source': filename,
                        'phase': phase,
                        'claimed_days': claimed_days,
                        'start_str': dates[0].strip(),
                        'end_str': dates[1].strip(),
                        'raw': date_range
                    })

    return entries


def parse_overview_days(filepath: str) -> int:
    """Parse the total days from the overview text."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Pattern: "a total of X calendar days"
    pattern = r'a total of (\d+) calendar days'
    match = re.search(pattern, content)
    if match:
        return int(match.group(1))
    return None


def verify_entry(entry: dict) -> dict:
    """Verify a single date/duration entry."""
    try:
        # Determine years from the date strings
        start_str = entry['start_str']
        end_str = entry['end_str']

        # Extract year from end date if present
        year_match = re.search(r'(\d{4})', end_str)
        end_year = int(year_match.group(1)) if year_match else 2025

        # Check if start has a year
        start_year_match = re.search(r'(\d{4})', start_str)
        if start_year_match:
            start_year = int(start_year_match.group(1))
        else:
            # Infer start year from end year
            start_year = end_year
            # If end is early in year (Jan-May) and start is late (Aug-Dec), start is previous year
            start_month_match = re.search(r'(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)', start_str)
            end_month_match = re.search(r'(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)', end_str)
            if start_month_match and end_month_match:
                start_months = ['Aug', 'Sep', 'Oct', 'Nov', 'Dec']
                end_months = ['Jan', 'Feb', 'Mar', 'Apr', 'May']
                if start_month_match.group(1) in start_months and end_month_match.group(1) in end_months:
                    start_year = end_year - 1

        start_date = parse_date(start_str, start_year)
        end_date = parse_date(end_str, end_year)

        actual_days = days_between(start_date, end_date)
        claimed_days = entry['claimed_days']

        return {
            **entry,
            'start_date': start_date,
            'end_date': end_date,
            'actual_days': actual_days,
            'match': actual_days == claimed_days,
            'error': None
        }
    except Exception as e:
        return {
            **entry,
            'start_date': None,
            'end_date': None,
            'actual_days': None,
            'match': False,
            'error': str(e)
        }


def main():
    # Determine paths
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    gantt_tex = project_root / 'sections' / '08_gantt.tex'
    gantt_dir = project_root / 'gantt'

    print(f"\n{BOLD}=== FTP DATE VERIFICATION REPORT ==={RESET}\n")

    errors_found = 0

    # Parse and verify schedule table
    print(f"{BOLD}Schedule Summary Table (sections/08_gantt.tex):{RESET}")
    schedule_entries = parse_schedule_table(gantt_tex)

    for entry in schedule_entries:
        result = verify_entry(entry)

        if result['error']:
            status = f"{RED}ERROR: {result['error']}{RESET}"
            errors_found += 1
        elif result['match']:
            status = f"{GREEN}OK{RESET}"
        else:
            status = f"{RED}MISMATCH (actual: {result['actual_days']} days){RESET}"
            errors_found += 1

        print(f"  {result['phase']:15} {result['claimed_days']:3} days | {result['raw']:35} | {status}")

    # Parse and verify Gantt headers
    print(f"\n{BOLD}Gantt File Headers (gantt/*.tex):{RESET}")
    gantt_entries = parse_gantt_headers(gantt_dir)

    for entry in gantt_entries:
        result = verify_entry(entry)

        if result['error']:
            status = f"{RED}ERROR: {result['error']}{RESET}"
            errors_found += 1
        elif result['match']:
            status = f"{GREEN}OK{RESET}"
        else:
            status = f"{RED}MISMATCH (actual: {result['actual_days']} days){RESET}"
            errors_found += 1

        print(f"  {result['source']:20} {result['phase']:10} {result['claimed_days']:3} days | {result['raw']:35} | {status}")

    # Cross-check: Gantt headers vs Schedule table
    print(f"\n{BOLD}Cross-Check: Gantt Headers vs Schedule Table:{RESET}")
    schedule_by_phase = {e['phase']: e for e in schedule_entries}

    for gantt_entry in gantt_entries:
        phase = gantt_entry['phase']
        if phase in schedule_by_phase:
            table_entry = schedule_by_phase[phase]
            if gantt_entry['claimed_days'] == table_entry['claimed_days']:
                status = f"{GREEN}MATCH{RESET}"
            else:
                status = f"{RED}MISMATCH (Gantt: {gantt_entry['claimed_days']}, Table: {table_entry['claimed_days']}){RESET}"
                errors_found += 1
            print(f"  {phase:15} | {status}")

    # Summary
    print(f"\n{BOLD}{'=' * 50}{RESET}")
    if errors_found == 0:
        print(f"{GREEN}All date verifications passed!{RESET}")
    else:
        print(f"{RED}ERRORS FOUND: {errors_found}{RESET}")

    return errors_found


if __name__ == '__main__':
    exit(main())
