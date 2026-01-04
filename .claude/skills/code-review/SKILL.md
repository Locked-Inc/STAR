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
5. **Generate report** - Produce structured markdown with findings

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
- [ ] All buffers statically allocated
- [ ] Fixed-size arrays with defined limits
- Search patterns: `\bmalloc\b`, `\bcalloc\b`, `\brealloc\b`, `\bfree\b`

### Rule 4: Keep Functions Short (~60 lines)
- [ ] Functions do not exceed 60 lines (excluding comments/blank lines)
- [ ] Each function has single responsibility
- Count: `wc -l` on function bodies

### Rule 5: Use Assertions/Validation
- [ ] Minimum 2 validation checks per function
- [ ] NULL pointer checks for all pointer parameters
- [ ] Range validation for numeric inputs
- [ ] State validation (e.g., `!handle->initialized`)
- Patterns: `RX_CHECK_`, `assert(`, `if\s*\([^)]*==\s*NULL`

### Rule 6: Declare Data at Smallest Scope
- [ ] Variables declared close to first use
- [ ] Loop variables declared in for statement
- [ ] No file-scope variables without `static`

### Rule 7: Check All Return Values
- [ ] All function returns checked or explicitly cast to `(void)`
- [ ] Error codes propagated appropriately
- [ ] Use of `RX_RETURN_ON_ERROR` macro

### Rule 8: Limit Preprocessor Use
- [ ] Prefer `enum` over `#define` for constants
- [ ] Prefer `static const` over `#define` for typed values
- [ ] No token pasting (`##`) except where necessary
- [ ] No recursive macros
- Good: `typedef enum { k_state_idle = 0 } state_t;`
- Bad: `#define STATE_IDLE 0`

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

## Report Format

Generate a markdown report with:

```markdown
# Code Review Report: [Directory/File]

## Summary
| Category | Status | Issues |
|----------|--------|--------|
| NASA Power of 10 | COMPLIANT/NON-COMPLIANT | N |
| SOLID Principles | COMPLIANT/NON-COMPLIANT | N |
| Style Guide | COMPLIANT/NON-COMPLIANT | N |

## NASA Power of 10 Findings

### Rule N: [Rule Name]
**Status:** COMPLIANT / NON-COMPLIANT / INTENTIONAL DEVIATION

**Findings:**
- `file.c:123` - Description of issue
- `file.c:456` - Description of issue

**Recommendation:** How to fix

## SOLID Principle Findings
[Similar structure]

## Style Guide Findings
[Similar structure]

## Positive Observations
- What the code does well
- Good patterns observed

## Recommendations
1. Priority fixes with code examples
2. References to documentation
```

## Reference Documentation

For detailed rule definitions, see:
- `docs/sections/06_nasa_power_of_10.tex` - Complete NASA Power of 10 rules
- `docs/sections/04_style_guide.tex` - Protocol Buffer and naming conventions
- `star-rx72n-firmware/CLAUDE.md` - RX72N-specific conventions
- `CLAUDE.md` - Project-wide standards
