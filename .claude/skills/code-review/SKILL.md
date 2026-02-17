---
name: code-review
description: Review code for NASA Power of 10, SOLID, and STAR standards compliance
disable-model-invocation: false
argument-hint: "[directory-or-file]"
---

# Code Review Skill

Performs automated code review for compliance with NASA Power of 10 rules, SOLID principles, and STAR coding standards.

## Usage

Invoke with a directory path:
```
Review the code in star-rx72n-firmware/lib/rx_motor/
```

Or review specific files:
```
Review rx_pid.c for NASA Power of 10 compliance
```

## Standards Reference

For complete rule definitions, examples, and rationale, see:

- **NASA Power of 10 Rules**: [CLAUDE.md](../../../CLAUDE.md#nasa-power-of-10-rules-star-implementation)
- **SOLID Principles for C**: [CLAUDE.md](../../../CLAUDE.md#solid-principles-for-c-star-implementation)
- **Code Style**: [CLAUDE.md](../../../CLAUDE.md#code-style)
- **Naming Conventions**: [CLAUDE.md](../../../CLAUDE.md#naming-conventions)
- **Doxygen Requirements**: [CLAUDE.md](../../../CLAUDE.md#doxygen-documentation-requirements)
- **Terminology Standard**: [CLAUDE.md](../../../CLAUDE.md#terminology-standard)

## Review Process

1. **Identify files** - Glob for `*.c` and `*.h` files in the specified directory
2. **Read each file** - Analyze source code content
3. **Apply NASA Power of 10 rules** - Check each rule (see CLAUDE.md for complete definitions)
4. **Apply SOLID principles** - Validate architecture patterns (see CLAUDE.md for examples)
5. **Check for tests** - If `tests/` directory exists, verify unit tests and integration tests are present
6. **Generate report** - Produce structured markdown with findings

## Detection Patterns (Quick Reference)

Use these patterns to quickly scan for common violations:

**NASA Power of 10:**
- Rule 1 (Control Flow): `\bgoto\b`, `\bsetjmp\b`, `\blongjmp\b`
- Rule 2 (Loop Bounds): `while\s*\(\s*1\s*\)`, `for\s*\(\s*;\s*;\s*\)`
- Rule 3 (Dynamic Memory): `\bmalloc\b`, `\bcalloc\b`, `\brealloc\b`, `\bfree\b`
- Rule 5 (Validation): `RX_CHECK_`, `assert\(`, `if\s*\([^)]*==\s*NULL`
- Rule 7 (Return Checks): Look for unchecked function calls
- Rule 8 (Preprocessor): `#define` for constants (should be enum), magic numbers

**Style Violations:**
- Magic numbers: Numeric literals without named enums
- Legacy terminology: `master`, `slave`, `MOSI`, `MISO`
- Incorrect prefixes: Functions without `snake_case`, types without `_t`
- Missing unit suffixes: Velocities without `_mps`, times without `_ms`

**SOLID Violations:**
- God classes/functions (> 60 lines, multiple responsibilities)
- Hardcoded values (should be configurable)
- Direct hardware dependencies (should use interfaces)
- Fat interfaces with many unused methods

## Report Format

Generate a markdown report with:

```markdown
# Code Review Report: [Directory/File]

## Summary
| Category | Status | Critical | High | Medium | Low |
|----------|--------|----------|------|--------|-----|
| NASA Power of 10 | COMPLIANT/NON-COMPLIANT | N | N | N | N |
| SOLID Principles | COMPLIANT/NON-COMPLIANT | N | N | N | N |
| Style Guide | COMPLIANT/NON-COMPLIANT | N | N | N | N |
| **Total** | | **N** | **N** | **N** | **N** |

### Severity Legend
- **CRITICAL**: Safety violation, undefined behavior, memory corruption (Rules 1, 3, 7, 9)
- **HIGH**: Verification issue, could cause runtime failure (Rules 2, 4, 5, 10)
- **MEDIUM**: Maintainability concern, style violation (Rule 6, 8, SOLID, naming)
- **LOW**: Minor style inconsistency, documentation improvement

## NASA Power of 10 Findings

### Rule N: [Rule Name]
**Status:** COMPLIANT / NON-COMPLIANT / INTENTIONAL DEVIATION

**Findings:**
- **[SEVERITY]** `file.c:123` - Description of issue
- **[SEVERITY]** `file.c:456` - Description of issue

**Recommendation:** How to fix

## SOLID Principle Findings
[Similar structure with severity tags]

## Style Guide Findings
[Similar structure with severity tags]

## Test Coverage
- [ ] Unit tests present for module
- [ ] Integration tests available
- [ ] Test coverage: N%

## Positive Observations
- What the code does well
- Good patterns observed

## Recommendations
1. **Critical Priority**: [List critical fixes first]
2. **High Priority**: [List high priority fixes]
3. **Medium/Low Priority**: [List other improvements]
```

## Reference Documentation

For detailed rule definitions, see:
- `docs/sections/06_nasa_power_of_10.tex` - Complete NASA Power of 10 rules
- `docs/sections/04_style_guide.tex` - Protocol Buffer and naming conventions
- `star-rx72n-firmware/CLAUDE.md` - RX72N-specific conventions
- `CLAUDE.md` - Project-wide standards
