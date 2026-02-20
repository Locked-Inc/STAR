---
name: pr
description: Create pull requests with proper title, description, and test plan. Use when user asks to create a PR, open a pull request, or submit changes for review.
disable-model-invocation: false
allowed-tools: Bash(git *), Bash(gh *)
---

# Pull Request Creation Skill

Create pull requests following STAR project conventions with comprehensive descriptions and test plans.

## Prerequisites

- Changes committed to a feature branch
- `gh` CLI installed and authenticated
- All tests passing locally

## PR Creation Workflow

When the user asks to create a pull request, follow these steps:

### Step 1: Analyze Branch State [PASS] READ-ONLY (Safe to run immediately)

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

### Step 2: Draft PR Title and Description [PASS] READ-ONLY (Safe to draft)

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

```

**IMPORTANT**: Do NOT add AI attribution to PR descriptions. Write natural, professional descriptions as if written by a human developer (per CLAUDE.md policy).

### Step 3: Verify Base Branch [PASS] READ-ONLY (Verify before push)

```bash
# Get current branch name
git branch --show-current

# Get default remote branch (usually 'main')
git remote show origin | grep 'HEAD branch'
```

**Default base branch**: `main` (ask user if targeting a different branch)

---

### Step 4: Push and Create PR

Run sequentially:

```bash
# 1. Push with upstream tracking
git push -u origin HEAD

# 2. Create PR with proper base and head branches
gh pr create \
  --base main \
  --head $(git branch --show-current) \
  --title "Your PR title here (max 70 chars)" \
  --body "$(cat <<'EOF'
## Summary
- Bullet point 1
- Bullet point 2
- Bullet point 3

## Test plan
- [x] Unit tests pass
- [x] Integration tests pass
- [x] Manual testing: describe what you tested
- [x] Code review checklist:
  - [x] NASA Power of 10 rules followed
  - [x] SOLID principles applied
  - [x] Doxygen documentation complete
  - [x] No magic numbers
  - [x] Inclusive terminology used

EOF
)"

# 3. View PR checks/CI status (optional)
gh pr checks

# 4. Get PR URL and open in browser
gh pr view --web
# OR get URL only:
gh pr view -q .url
```

**Notes**:
- Use `--base <branch>` if targeting non-default branch
- `HEAD` refers to current branch (no need to hardcode branch name)
- `gh pr view --web` opens browser, `-q .url` prints URL only
- Check `gh pr status` to verify PR creation success

## Examples

### Example 1: Feature Addition

```bash
# Step 1: Analyze changes (READ-ONLY)
git status
git log main..HEAD --oneline
git diff main...HEAD

# Step 2: Draft title and description (READ-ONLY)
# Title: "Add PID controller for motor velocity control"
# Description: See below

# Step 3: Verify base branch (READ-ONLY)
git branch --show-current  # Returns: feature/pid-controller
git remote show origin | grep 'HEAD branch'  # Returns: main

# Step 4: Push and create PR
git push -u origin HEAD

gh pr create \
  --base main \
  --head feature/pid-controller \
  --title "Add PID controller for motor velocity control" \
  --body "$(cat <<'EOF'
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

EOF
)"

# Return URL
gh pr view -q .url
```

### Example 2: Bug Fix

```bash
git push -u origin HEAD

gh pr create \
  --base main \
  --head $(git branch --show-current) \
  --title "Fix SPI timeout calculation" \
  --body "$(cat <<'EOF'
## Summary
- Fixes SPI timeout calculation using milliseconds instead of microseconds
- Prevents premature timeouts during large transfers

## Test plan
- [x] Unit tests pass
- [x] Manual testing: 1KB SPI transfer completes without timeout
- [x] Verified timeout triggers correctly after 1000ms delay
- [x] Code review: No new violations

EOF
)"

gh pr view --web
```

### Example 3: Refactoring

```bash
git push -u origin HEAD

gh pr create \
  --base main \
  --head $(git branch --show-current) \
  --title "Refactor motor control state machine" \
  --body "$(cat <<'EOF'
## Summary
- Simplifies state machine transitions with lookup table
- Reduces cyclomatic complexity from 15 to 8
- No behavioral changes (pure refactoring)

## Test plan
- [x] All existing tests pass (no test changes needed)
- [x] State transition coverage remains 100%
- [x] Manual testing: All motor control modes work identically
- [x] Code review: Improved readability, same safety guarantees

EOF
)"

gh pr view -q .url
```

## Important Notes

- **Analyze ALL commits**: Don't just look at the latest commit - review the full diff from the base branch
- **Title length**: Keep under 70 characters for readability
- **Description detail**: Use description for details, not title
- **Test plan**: Be specific about what was tested and how
- **Base branch**: Default to `main`, but check with user if uncertain
- **Skill permissions**: Keep `allowed-tools` scoped to `Bash(git *)` and `Bash(gh *)`
- **No force push**: Use `git push` not `git push --force` unless explicitly requested and approved
- **No AI attribution**: Never add "Generated by Claude Code" or similar footers (applies to both commits AND PRs)

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
