# Code Review Agent

Performs automated code review for compliance with NASA Power of 10 rules and SOLID principles.

## Usage

Invoke with a directory path:
```
Review the code in star-rx72n-firmware/lib/rx_motor/
```

Or review specific files:
```
Review rx_pid.c for NASA Power of 10 compliance
```

## Review Process

1. **Identify files** - Glob for `*.c` and `*.h` files in the specified directory
2. **Read each file** - Analyze source code content
3. **Apply NASA Power of 10 rules** - Check each rule from the checklist
4. **Apply SOLID principles** - Validate architecture patterns
5. **Check for tests** - If `tests/` directory exists, verify unit tests and integration tests are present
6. **Generate report** - Produce structured markdown with findings

## NASA Power of 10 Rules Checklist

### Rule 1: Simplify Control Flow
- [ ] No `goto` statements
- [ ] No `setjmp`/`longjmp`
- [ ] No recursion (direct or indirect)
- Search patterns: `\bgoto\b`, `\bsetjmp\b`, `\blongjmp\b`

### Rule 2: Fixed Loop Upper-Bounds
- [ ] All loops have fixed iteration limits
- [ ] No `while(1)` without bounded exit (except main loop)
- [ ] Loop counters use explicit types (uint32_t, not size_t)
- Red flags: `while\s*\(\s*1\s*\)`, `for\s*\(\s*;\s*;\s*\)`

### Rule 3: No Dynamic Memory After Initialization
- [ ] No `malloc`/`calloc`/`realloc`/`free` in runtime code
- [ ] **Exception**: Dynamic allocation is permitted during initialization phase (before main control loop starts)
- [ ] All runtime buffers statically allocated
- [ ] Fixed-size arrays with defined limits
- Search patterns: `\bmalloc\b`, `\bcalloc\b`, `\brealloc\b`, `\bfree\b`
- **Important**: Verify allocation occurs in `*_init()` functions, not in control loops or ISRs

### Rule 4: Keep Functions Short (~60 lines)
- [ ] Functions do not exceed 60 lines (excluding comments/blank lines)
- [ ] Each function has single responsibility
- Count: `wc -l` on function bodies

### Rule 5: Use Assertions/Validation
- [ ] Minimum 2 validation checks per function
- [ ] **Pre-conditions**: NULL pointer checks for all pointer parameters
- [ ] **Pre-conditions**: Range validation for numeric inputs
- [ ] **Pre-conditions**: State validation (e.g., `!handle->initialized`)
- [ ] **Post-conditions**: Validate outputs and invariants where applicable
- Patterns: `RX_CHECK_`, `assert(`, `if\s*\([^)]*==\s*NULL`
- **Good example**: See `star-rx72n-firmware/lib/rx_pid/src/rx_pid.c::rx_pid_compute()` for comprehensive pre-condition and post-condition validation

### Rule 6: Declare Data at Smallest Scope
- [ ] Variables declared close to first use
- [ ] Loop variables declared in for statement
- [ ] No file-scope variables without `static`

### Rule 7: Check All Return Values
- [ ] All function returns checked or explicitly cast to `(void)`
- [ ] Error codes propagated appropriately
- [ ] Use of `RX_RETURN_ON_ERROR` macro

### Rule 8: Limit Preprocessor Use
- [ ] **ALWAYS use `enum` for integer constants** (never `#define`)
- [ ] Use `static const` ONLY for floating-point values (enum can't hold floats)
- [ ] Macros ONLY allowed for: (1) reducing duplicated code, (2) conditional compilation, (3) hardware addresses, (4) build flags
- [ ] **FORBIDDEN**: Macros for simple constants or backward compatibility (no releases = no compatibility needed)
- [ ] **NO MAGIC NUMBERS**: All numeric literals must be named enums (including array indices, bit shifts, offsets)
- [ ] No token pasting (`##`) except in allowed macro cases
- [ ] No recursive macros
- Good: `typedef enum { k_timeout_ms = 1000, k_max_retries = 3 } limits_t;`
- Good: `typedef enum { k_idx_high_byte = 0, k_idx_low_byte = 1 } be16_idx_t;`
- Good: `typedef enum { k_shift_enable = 7, k_shift_mode = 3 } reg_shifts_t;`
- Good: `#define RX_RETURN_ON_ERROR(err, tag, msg) /* multi-line validation */`
- Bad: `buf[0] = (val >> 8);` (magic 0 and 8 - use enums!)
- Bad: `#define TIMEOUT_MS 1000` (should be enum!)
- Bad: `#define MAX_VELOCITY 2.5f` (should be static const float!)
- Bad: `#define old_func new_func` (no backward compatibility - just update call sites!)

### Rule 9: Restrict Pointer Use
- [ ] Maximum one level of dereferencing (exception: DIP interfaces)
- [ ] Function pointers ONLY for Dependency Inversion Pattern
- [ ] Document all function pointer interfaces

### Rule 10: Compile with Maximum Warnings
- [ ] Check CMakeLists.txt for `-Wall -Wextra -Werror`
- [ ] No warning suppressions without justification

## SOLID Principles Checklist

### Single Responsibility (S)
- [ ] Each file/module has one purpose
- [ ] Functions do one thing well
- [ ] Clear separation of concerns

### Open/Closed (O)
- [ ] Modules extensible without modification
- [ ] Configuration via parameters, not code changes

### Liskov Substitution (L)
- [ ] Interface implementations are interchangeable
- [ ] Mocks can substitute real implementations

### Interface Segregation (I)
- [ ] Small, focused interfaces
- [ ] No "fat" interfaces with unused methods
- [ ] Separate read/write interfaces where appropriate

### Dependency Inversion (D)
- [ ] High-level modules don't depend on low-level details
- [ ] Use function pointer interfaces for hardware abstraction
- [ ] Testable via mock injection
- Pattern: `typedef struct { rx_err_t (*operation)(void* ctx, ...); void* ctx; } interface_t;`

## C Style Guide Checklist

### Naming Conventions
- [ ] Functions/variables: `snake_case`
- [ ] Macros/constants: `SCREAMING_SNAKE_CASE`
- [ ] Types: `snake_case_t`
- [ ] Static functions: `internal_` prefix
- [ ] Private functions: `priv_` prefix
- [ ] Static variables: `s_` prefix
- [ ] Global variables: `g_` prefix (avoid globals)
- [ ] Enum constants: `k_` prefix for values

### Unit Suffixes (MKS System)
- [ ] `_m` for meters
- [ ] `_mps` for meters/second
- [ ] `_rad` for radians
- [ ] `_celsius` for temperature
- [ ] `_ms` for milliseconds
- [ ] `_us` for microseconds
- [ ] `_ma` for milliamps
- [ ] `_mv` for millivolts

### Inclusive Terminology
- [ ] Use Controller/Peripheral (not master/slave)
- [ ] Use COPI/CIPO (not MOSI/MISO)
- [ ] Use Primary/Main (not master)

### File Documentation
- [ ] First line contains path comment: `/* path/to/file.ext */`
- [ ] Doxygen header with required tags: `@file`, `@brief`, `@date`, `@copyright`
- [ ] `@details` section for complex modules
- [ ] `@code` example usage for public API headers
- [ ] Date format: `YYYY-MM-DD`
- [ ] Copyright: `Copyright (c) 2026 STAR Project`
- [ ] No `@author` or `@version` tags (project policy)

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
