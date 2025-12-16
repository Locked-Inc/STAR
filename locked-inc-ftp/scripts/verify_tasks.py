#!/usr/bin/env python3
"""
FTP Task Count Verification Script

Verifies that all task counts are accurate and consistent throughout the FTP document.
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


def load_activities(filepath: str) -> dict:
    """Load activities from YAML file."""
    with open(filepath, 'r') as f:
        return yaml.safe_load(f)


def count_leaf_tasks(activities: dict) -> dict:
    """Count tasks that have duration_days (leaf nodes) by phase."""
    counts = defaultdict(int)
    task_details = defaultdict(list)

    for task_id, task in activities.items():
        if isinstance(task, dict) and 'duration_days' in task:
            # Get phase prefix (e.g., "1.1" from "1.1.1.1")
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


def count_all_tasks(activities: dict) -> dict:
    """Count all tasks (leaf + parent) by phase."""
    counts = defaultdict(int)

    for task_id, task in activities.items():
        if isinstance(task, dict):
            task_id_str = str(task_id)
            parts = task_id_str.split('.')
            if len(parts) >= 2:
                phase = f"{parts[0]}.{parts[1]}"
                counts[phase] += 1

    return dict(counts)


def parse_gantt_task_counts(gantt_dir: str) -> dict:
    """Parse task counts from Gantt file headers."""
    counts = {}

    # Pattern: Total Tasks: X
    pattern = r'Total Tasks:\}\s*(\d+)'

    for phase, filename in GANTT_FILES.items():
        filepath = os.path.join(gantt_dir, filename)
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                content = f.read()

            match = re.search(pattern, content)
            if match:
                counts[phase] = int(match.group(1))

    return counts


def parse_overview_task_count(filepath: str) -> int:
    """Parse the total task count from the overview text."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Pattern: "showing all X activities"
    pattern = r'showing all (\d+) activities'
    match = re.search(pattern, content)
    if match:
        return int(match.group(1))
    return None


def parse_dependencies_count(filepath: str) -> int:
    """Parse the dependencies count from the Gantt section."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Pattern: "All X dependencies"
    pattern = r'All (\d+) dependencies'
    match = re.search(pattern, content)
    if match:
        return int(match.group(1))
    return None


def count_dependencies_in_yaml(activities: dict) -> int:
    """Count total dependencies in the activities YAML."""
    total_deps = 0
    for task_id, task in activities.items():
        if isinstance(task, dict) and 'depends_on' in task:
            deps = task['depends_on']
            if isinstance(deps, list):
                total_deps += len(deps)
    return total_deps


def main():
    # Determine paths
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    activities_file = project_root / 'FTP' / 'data' / 'activities_cache.yaml'
    gantt_tex = project_root / 'sections' / '08_gantt.tex'
    gantt_dir = project_root / 'gantt'

    print(f"\n{BOLD}=== FTP TASK COUNT VERIFICATION REPORT ==={RESET}\n")

    errors_found = 0

    # Load activities from YAML
    if not activities_file.exists():
        print(f"{RED}ERROR: activities_cache.yaml not found at {activities_file}{RESET}")
        return 1

    activities = load_activities(activities_file)
    leaf_counts, task_details = count_leaf_tasks(activities)
    all_counts = count_all_tasks(activities)

    # Display YAML counts
    print(f"{BOLD}Source: activities_cache.yaml{RESET}")
    print(f"  {'Phase':<15} {'Leaf Tasks':>12} {'All Tasks':>12}")
    print(f"  {'-'*15} {'-'*12} {'-'*12}")

    total_leaf = 0
    total_all = 0
    for phase_id in sorted(PHASE_NAMES.keys()):
        phase_name = PHASE_NAMES[phase_id]
        leaf = leaf_counts.get(phase_id, 0)
        all_tasks = all_counts.get(phase_id, 0)
        total_leaf += leaf
        total_all += all_tasks
        print(f"  {phase_id} {phase_name:<10} {leaf:>12} {all_tasks:>12}")

    print(f"  {'-'*15} {'-'*12} {'-'*12}")
    print(f"  {'TOTAL':<15} {total_leaf:>12} {total_all:>12}")

    # Parse Gantt file headers
    print(f"\n{BOLD}Gantt File Headers vs YAML Leaf Tasks:{RESET}")
    gantt_counts = parse_gantt_task_counts(gantt_dir)

    phase_id_map = {'Research': '1.1', 'Design': '1.2', 'Build': '1.3', 'Test': '1.4', 'Close': '1.5'}

    for phase_name, phase_id in phase_id_map.items():
        gantt_count = gantt_counts.get(phase_name, 'N/A')
        yaml_count = leaf_counts.get(phase_id, 0)

        if gantt_count == 'N/A':
            status = f"{YELLOW}NOT FOUND{RESET}"
        elif gantt_count == yaml_count:
            status = f"{GREEN}MATCH{RESET}"
        else:
            status = f"{RED}MISMATCH{RESET}"
            errors_found += 1

        print(f"  {GANTT_FILES.get(phase_name, 'N/A'):<22} Gantt: {str(gantt_count):>3} | YAML: {yaml_count:>3} | {status}")

    # Check overview claim
    print(f"\n{BOLD}Overview Claim (sections/08_gantt.tex):{RESET}")
    overview_count = parse_overview_task_count(gantt_tex)

    if overview_count:
        # Compare with total leaf tasks
        if overview_count == total_leaf:
            status = f"{GREEN}MATCH (leaf tasks){RESET}"
        elif overview_count == total_all:
            status = f"{GREEN}MATCH (all tasks){RESET}"
        else:
            status = f"{RED}MISMATCH (claimed: {overview_count}, leaf: {total_leaf}, all: {total_all}){RESET}"
            errors_found += 1
        print(f"  Claimed activities: {overview_count} | {status}")
    else:
        print(f"  {YELLOW}Could not parse overview task count{RESET}")

    # Check dependencies count
    print(f"\n{BOLD}Dependencies Count:{RESET}")
    claimed_deps = parse_dependencies_count(gantt_tex)
    actual_deps = count_dependencies_in_yaml(activities)

    if claimed_deps:
        if claimed_deps == actual_deps:
            status = f"{GREEN}MATCH{RESET}"
        else:
            status = f"{RED}MISMATCH{RESET}"
            errors_found += 1
        print(f"  Claimed: {claimed_deps} | Actual (YAML): {actual_deps} | {status}")
    else:
        print(f"  Actual dependencies in YAML: {actual_deps}")

    # Detailed task list per phase (optional, can be verbose)
    print(f"\n{BOLD}Task Details by Phase:{RESET}")
    for phase_id in sorted(task_details.keys()):
        tasks = task_details[phase_id]
        phase_name = PHASE_NAMES.get(phase_id, phase_id)
        total_duration = sum(t['duration'] for t in tasks)
        print(f"  {phase_id} {phase_name}: {len(tasks)} tasks, {total_duration} total days")

    # Summary
    print(f"\n{BOLD}{'=' * 50}{RESET}")
    if errors_found == 0:
        print(f"{GREEN}All task count verifications passed!{RESET}")
    else:
        print(f"{RED}ERRORS FOUND: {errors_found}{RESET}")

    return errors_found


if __name__ == '__main__':
    exit(main())
