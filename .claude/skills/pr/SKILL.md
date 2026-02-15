---
name: pr
description: Create a pull request with proper title, description, and test plan following STAR conventions
disable-model-invocation: true
allowed-tools: [Bash(git *), Bash(gh *), Read, Grep]
context: main
---

# Pull Request Creation Skill

Create pull requests following STAR project conventions with comprehensive descriptions and test plans.

## Prerequisites

- Changes committed to a feature branch
- `gh` CLI installed and authenticated
- All tests passing locally

## PR Creation Workflow

When the user asks to create a pull request, follow these steps:

### Step 1: Analyze Branch State (Parallel)

Run these commands in parallel to understand the full scope of changes:

```bash
# See all untracked files (NEVER use -uall flag)
git status

# See both staged and unstaged changes
git diff HEAD

# Check if branch tracks remote and is up to date
git status -sb

# Get commit history from divergence point (replace 'main' with actual base branch)
git log main..HEAD --oneline

# See all changes since divergence from base branch
git diff main...HEAD
```

**IMPORTANT**: Analyze ALL commits that will be included in the PR, not just the latest commit!

### Step 2: Draft PR Title and Description

Based on analysis of ALL changes:

**Title** (max 70 characters):
- Keep short and descriptive
- Use imperative mood ("Add feature" not "Added feature")
- Don't include implementation details (save for description)

**Description format**:
```markdown
## Summary
<1-3 bullet points summarizing what changed and why>

## Test plan
- [ ] Unit tests pass (`go test ./...` or equivalent)
- [ ] Integration tests pass
- [ ] Manual testing: <describe what you tested>
- [ ] Code review checklist:
  - [ ] NASA Power of 10 rules followed
  - [ ] SOLID principles applied
  - [ ] Doxygen documentation complete
  - [ ] No magic numbers
  - [ ] Inclusive terminology used

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

**Note**: The "Generated with Claude Code" footer is ONLY for PR descriptions, NEVER for commit messages.

### Step 3: Push and Create PR (Sequential if needed)

```bash
# Create branch if needed (optional)
git checkout -b feature/my-feature

# Push with upstream tracking if needed
git push -u origin feature/my-feature

# Create PR using heredoc for description
gh pr create --title "Add PID controller implementation" --body "$(cat <<'EOF'
## Summary
- Implements discrete-time PID algorithm for motor velocity control
- Adds anti-windup clamping and derivative low-pass filtering
- Includes comprehensive unit tests with 95% coverage

## Test plan
- [x] Unit tests pass (`cd e2-studio-star-rx72n-firmware/tests && ctest`)
- [x] Simulator tests pass (no hardware errors)
- [x] Manual testing: PID tuning with step response verified in MATLAB
- [x] Code review checklist:
  - [x] NASA Power of 10 rules followed
  - [x] SOLID principles applied (dependency injection for mocking)
  - [x] Doxygen documentation complete (100% function coverage)
  - [x] No magic numbers (all constants in typed enums)
  - [x] Inclusive terminology used

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

### Step 4: Return PR URL

After successful creation, return the PR URL to the user so they can view it.

## Examples

### Example 1: Feature Addition

```bash
# Step 1: Analyze changes
git status
git log main..HEAD --oneline
git diff main...HEAD

# Step 2: Push if needed
git push -u origin feature/pid-controller

# Step 3: Create PR
gh pr create --title "Add PID controller for motor velocity control" --body "$(cat <<'EOF'
## Summary
- Implements discrete-time PID controller with anti-windup
- Adds unit tests with 95% code coverage
- Includes MATLAB tuning scripts for gain calculation

## Test plan
- [x] Unit tests pass (15/15 tests passing)
- [x] Simulator validation complete
- [x] MATLAB step response matches expected behavior
- [x] Code review: NASA Power of 10 compliant
- [x] Documentation: All functions have Doxygen comments

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

### Example 2: Bug Fix

```bash
gh pr create --title "Fix SPI timeout calculation" --body "$(cat <<'EOF'
## Summary
- Fixes SPI timeout calculation using milliseconds instead of microseconds
- Prevents premature timeouts during large transfers

## Test plan
- [x] Unit tests pass
- [x] Manual testing: 1KB SPI transfer completes without timeout
- [x] Verified timeout triggers correctly after 1000ms delay
- [x] Code review: No new violations

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

### Example 3: Refactoring

```bash
gh pr create --title "Refactor motor control state machine" --body "$(cat <<'EOF'
## Summary
- Simplifies state machine transitions with lookup table
- Reduces cyclomatic complexity from 15 to 8
- No behavioral changes (pure refactoring)

## Test plan
- [x] All existing tests pass (no test changes needed)
- [x] State transition coverage remains 100%
- [x] Manual testing: All motor control modes work identically
- [x] Code review: Improved readability, same safety guarantees

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

## Important Notes

- **Analyze ALL commits**: Don't just look at the latest commit - review the full diff from the base branch
- **Title length**: Keep under 70 characters for readability
- **Description detail**: Use description for details, not title
- **Test plan**: Be specific about what was tested and how
- **Base branch**: Default to `main`, but check with user if uncertain
- **No force push**: Use `git push` not `git push --force` unless explicitly requested and approved
- **PR footer**: The "Generated with Claude Code" footer is acceptable for PRs (unlike commit messages)

## Troubleshooting

**Problem**: Branch not tracking remote
```bash
# Solution: Push with -u flag
git push -u origin <branch-name>
```

**Problem**: PR already exists for branch
```bash
# Solution: Update existing PR or create from different branch
gh pr list  # Check existing PRs
```

**Problem**: Uncommitted changes
```bash
# Solution: Commit first, then create PR
# Use /commit skill to create proper commit
```

## gh CLI Tips

```bash
# View PR in browser after creation
gh pr view --web

# List all open PRs
gh pr list

# Check PR status
gh pr status

# View PR checks/CI status
gh pr checks
```
