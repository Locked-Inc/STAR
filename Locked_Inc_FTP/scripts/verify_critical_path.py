#!/usr/bin/env python3
"""
FTP Critical Path Verification Script

Verifies critical path metrics throughout the FTP document.
"""

import re
import os
import yaml
from pathlib import Path
from collections import defaultdict

# ANSI colors for terminal output
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
RESET = "\033[0m"
BOLD = "\033[1m"


def load_activities(filepath: str) -> dict:
    """Load activities from YAML file."""
    with open(filepath, 'r') as f:
        return yaml.safe_load(f)


def count_critical_path_tasks_in_gantt(gantt_dir: str) -> dict:
    """Count tasks marked with red (critical path) in Gantt files."""
    counts = {}
    task_ids = {}

    gantt_files = [
        ('gantt_research.tex', 'Research'),
        ('gantt_design.tex', 'Design'),
        ('gantt_build.tex', 'Build'),
        ('gantt_test.tex', 'Test'),
        ('gantt_close.tex', 'Close'),
    ]

    # Pattern for critical path tasks: bar/.append style={fill=red!50}
    critical_pattern = r'\\ganttbar\[.*fill=red.*\]\{([^}]+)\}'

    # Pattern for claimed count in notes
    note_pattern = r'critical path \((\d+) tasks'

    for filename, phase in gantt_files:
        filepath = os.path.join(gantt_dir, filename)
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                content = f.read()

            # Count actual red tasks
            critical_tasks = re.findall(critical_pattern, content)
            counts[phase] = {
                'actual': len(critical_tasks),
                'tasks': critical_tasks
            }

            # Get claimed count from note
            note_match = re.search(note_pattern, content)
            if note_match:
                counts[phase]['claimed'] = int(note_match.group(1))
            else:
                counts[phase]['claimed'] = None

            # Extract task IDs
            task_ids[phase] = []
            for task in critical_tasks:
                # Extract task ID like "1.1.1.1" from "1.1.1.1: Task name..."
                id_match = re.match(r'([\d.]+):', task)
                if id_match:
                    task_ids[phase].append(id_match.group(1))

    return counts, task_ids


def get_task_durations(activities: dict, task_ids: list) -> int:
    """Get total duration of specific tasks."""
    total_duration = 0
    for task_id in task_ids:
        if task_id in activities and 'duration_days' in activities[task_id]:
            total_duration += activities[task_id]['duration_days']
    return total_duration


def count_leaf_tasks(activities: dict) -> int:
    """Count all leaf tasks (tasks with duration_days)."""
    count = 0
    for task_id, task in activities.items():
        if isinstance(task, dict) and 'duration_days' in task:
            count += 1
    return count


def parse_critical_path_section(filepath: str) -> dict:
    """Parse the critical path section for claimed metrics."""
    with open(filepath, 'r') as f:
        content = f.read()

    metrics = {}

    # Look for the metrics table
    # Pattern: Total Activities on Critical Path & X \\
    patterns = {
        'total_on_cp': r'Total Activities on Critical Path\s*&\s*(\d+)',
        'cp_duration': r'Critical Path Duration\s*&\s*(\d+)\s*days',
        'total_activities': r'Total Project Activities\s*&\s*(\d+)',
        'percentage': r'Percentage on Critical Path\s*&\s*(\d+)%',
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            metrics[key] = int(match.group(1))

    return metrics


def main():
    # Determine paths
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    activities_file = project_root / 'FTP' / 'data' / 'activities_cache.yaml'
    gantt_dir = project_root / 'gantt'
    critical_path_tex = project_root / 'sections' / '10_critical_path.tex'

    print(f"\n{BOLD}=== FTP CRITICAL PATH VERIFICATION REPORT ==={RESET}\n")

    errors_found = 0

    # Load activities
    activities = load_activities(activities_file)

    # Count critical path tasks from Gantt files
    print(f"{BOLD}Critical Path Tasks by Phase (from Gantt files):{RESET}")
    cp_counts, cp_task_ids = count_critical_path_tasks_in_gantt(gantt_dir)

    total_cp_tasks = 0
    total_cp_duration = 0

    for phase, data in cp_counts.items():
        actual = data['actual']
        claimed = data['claimed']
        total_cp_tasks += actual

        # Calculate duration for this phase's critical path tasks
        phase_duration = get_task_durations(activities, cp_task_ids.get(phase, []))
        total_cp_duration += phase_duration

        if claimed is None:
            status = f"{YELLOW}NO CLAIM{RESET}"
        elif actual == claimed:
            status = f"{GREEN}MATCH{RESET}"
        else:
            status = f"{RED}MISMATCH{RESET}"
            errors_found += 1

        print(f"  {phase:10} Actual: {actual:2} | Claimed: {str(claimed):>4} | Duration: {phase_duration:3} days | {status}")

    print(f"  {'-'*60}")
    print(f"  {'TOTAL':10} {total_cp_tasks:2} tasks | Duration: {total_cp_duration} days")

    # Get total project activities
    total_activities = count_leaf_tasks(activities)

    # Calculate percentage
    if total_activities > 0:
        percentage = round((total_cp_tasks / total_activities) * 100)
    else:
        percentage = 0

    # Parse claimed metrics from critical path section
    print(f"\n{BOLD}Claimed Metrics (from sections/10_critical_path.tex):{RESET}")
    claimed_metrics = parse_critical_path_section(critical_path_tex)

    if claimed_metrics:
        for key, value in claimed_metrics.items():
            print(f"  {key}: {value}")
    else:
        print(f"  {YELLOW}Could not parse metrics from critical path section{RESET}")

    # Verification
    print(f"\n{BOLD}Verification:{RESET}")

    # 1. Total Activities on Critical Path
    claimed_cp = claimed_metrics.get('total_on_cp')
    if claimed_cp:
        if claimed_cp == total_cp_tasks:
            status = f"{GREEN}MATCH{RESET}"
        else:
            status = f"{RED}MISMATCH{RESET}"
            errors_found += 1
        print(f"  Total on Critical Path: Claimed {claimed_cp} | Actual {total_cp_tasks} | {status}")

    # 2. Total Project Activities
    claimed_total = claimed_metrics.get('total_activities')
    if claimed_total:
        if claimed_total == total_activities:
            status = f"{GREEN}MATCH{RESET}"
        else:
            status = f"{RED}MISMATCH{RESET}"
            errors_found += 1
        print(f"  Total Project Activities: Claimed {claimed_total} | Actual {total_activities} | {status}")

    # 3. Percentage on Critical Path
    claimed_pct = claimed_metrics.get('percentage')
    if claimed_pct:
        if claimed_pct == percentage:
            status = f"{GREEN}MATCH{RESET}"
        else:
            status = f"{RED}MISMATCH (actual: {percentage}%){RESET}"
            errors_found += 1
        print(f"  Percentage on Critical Path: Claimed {claimed_pct}% | Actual {percentage}% | {status}")

    # 4. Critical Path Duration
    claimed_duration = claimed_metrics.get('cp_duration')
    if claimed_duration:
        print(f"  Critical Path Duration: Claimed {claimed_duration} days | Calculated from tasks: {total_cp_duration} days")
        print(f"    {YELLOW}Note: Duration should be calculated from actual schedule, not sum of task durations{RESET}")

    # List all critical path task IDs
    print(f"\n{BOLD}All Critical Path Task IDs:{RESET}")
    all_cp_ids = []
    for phase, ids in cp_task_ids.items():
        all_cp_ids.extend(ids)
        if ids:
            print(f"  {phase}: {', '.join(ids)}")

    print(f"\n  Total unique CP tasks: {len(all_cp_ids)}")

    # Summary
    print(f"\n{BOLD}{'=' * 50}{RESET}")
    if errors_found == 0:
        print(f"{GREEN}All critical path verifications passed!{RESET}")
    else:
        print(f"{RED}ERRORS FOUND: {errors_found}{RESET}")

    return errors_found


if __name__ == '__main__':
    exit(main())
