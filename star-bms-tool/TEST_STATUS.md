# STAR BMS Tool - Test Status Report

**Date**: January 10, 2026
**Status**: In Progress - Test Suite Expansion

## Summary

Created comprehensive test suite covering all BMS features. Initial run shows failures that need investigation and fixes.

## Experimental Features

✅ **Calibration Wizard**: Now hidden behind experimental feature flag
- Feature flag added to `Cargo.toml`: `experimental = []`
- Runtime check via `is_experimental_enabled()` command
- UI conditionally shows tab only when compiled with `--features experimental`
- To enable: `cargo tauri dev --features experimental`

## Test Files Created

| Test File | Purpose | Status |
|-----------|---------|--------|
| `connection.spec.ts` | Connection management | ✅ Existing |
| `telemetry.spec.ts` | Telemetry reading | ✅ Existing |
| `cell-voltages.spec.ts` | Basic cell voltage tests | ✅ Existing |
| `cell-voltages-validation.spec.ts` | Cell validation & clamping | ✅ **5/5 passing** |
| `device-info.spec.ts` | Device information | ✅ Existing |
| `registers.spec.ts` | Register read/write | ✅ Existing |
| `manufacturer.spec.ts` | Manufacturer Access | ✅ Existing |
| `e2e-workflow.spec.ts` | End-to-end workflows | ✅ Existing |
| `protection-status.spec.ts` | Protection flags monitoring | ⏳ **NEW - Not yet run** |
| `data-flash.spec.ts` | Data Flash programming | ⏳ **NEW - Partial failures** |
| `chemistry-profiles.spec.ts` | Battery chemistry profiles | ❌ **NEW - 13/22 failing** |
| `fet-control-balancing.spec.ts` | FET control & cell balancing | ⏳ **NEW - Not yet run** |
| `custom-inputs.spec.ts` | All custom/manual inputs | ⏳ **NEW - Not yet run** |

## Known Issues

### 1. Chemistry Profiles Tab Issues (13 failures)
- **Problem**: Tests can't find dropdown element
- **Likely cause**: Tab may not exist or UI structure differs from test expectations
- **Error**: `strict mode violation: locator('select').first() resolved to 0 elements`
- **Fix needed**: Verify Chemistry Profiles tab exists and update locators

### 2. Locator Syntax Errors
- **Problem**: Mixing CSS selectors with text regex
- **Bad**: `.locator('.description, text=/common.*consumer/i')`
- **Good**: `.locator('.description').or(page.locator('text=/common.*consumer/i'))`
- **Fix needed**: Update all mixed locators to use `.or()` method

### 3. Strict Mode Violations
- **Problem**: Multiple elements match single locator
- **Example**: `locator('text=/mV|V/')` matches 12 elements
- **Fix needed**: Add `.first()` or use more specific selectors

### 4. Data Flash Validation
- **Problem**: Class number validation test failing
- **Error**: Can't find error message for invalid class 256
- **Fix needed**: Verify backend validates class range and displays error

## Cell Voltage Clamping Fix

✅ **Successfully Fixed** - Validation now clamps to device maximum before checking protocol limit

**Old Behavior**:
- User requests 30 cells → Error "exceeds protocol maximum of 16"
- Confusing: Shows warning "will adjust to 4" but fails with error

**New Behavior**:
- User requests 30 cells → Clamped to 4 → Success
- Shows warning "adjusted to 4", no error
- Test coverage: 5/5 tests passing

**Backend Changes**:
```rust
// Clamp to device capabilities first
let clamped = if requested > device_cells {
    device_cells
} else {
    requested
};
// Then check protocol maximum
if clamped > MAX_CELLS_PROTOCOL {
    return Err(...)
}
Ok(clamped)
```

## Next Steps

### Immediate (Required for all tests passing)

1. **Verify UI Structure**
   - Check if Chemistry Profiles tab actually exists
   - Check if Data Flash tab has expected elements
   - Check if FET controls are in header or tabs

2. **Fix Test Locators**
   - Replace mixed CSS/text selectors with `.or()` pattern
   - Add `.first()` to ambiguous locators
   - Use more specific selectors

3. **Backend Validation**
   - Ensure Data Flash class validation (0-255)
   - Ensure manufacturer access subcommand validation
   - Ensure all error messages are displayed in UI

4. **Run Full Test Suite**
   - `npm test` to get complete failure list
   - Fix failures one by one
   - Target: 100% passing

### Future Enhancements

1. **Edge Case Coverage**
   - Disconnection during operations
   - Invalid data formats
   - Boundary conditions (max values, empty strings)
   - Race conditions (rapid clicks)

2. **Performance Tests**
   - Auto-refresh reliability
   - Large data handling (16 cells)
   - Memory leaks during long sessions

3. **Accessibility Tests**
   - Keyboard navigation
   - Screen reader compatibility
   - Color contrast for indicators

## How to Run Tests

### All tests:
```bash
npm test
```

### Specific test file:
```bash
npm test tests/ui/cell-voltages-validation.spec.ts
```

### With UI:
```bash
npm run test:ui
```

### In headed mode (see browser):
```bash
npm run test:headed
```

### With experimental features:
```bash
# 1. Build with experimental flag
cargo tauri dev --features experimental

# 2. Run tests (in separate terminal)
npm test
```

## Test Execution Notes

- **Mock Device**: Tests use mock backend via `test-setup.ts`
- **Mock Device Simulation**: Clamping logic matches real backend
- **Experimental Features**: Disabled by default in tests
- **Connection Required**: Most tests require mock connection first

## Coverage Goals

- ✅ **Connection Management**: Full coverage
- ✅ **Telemetry**: Full coverage
- ✅ **Cell Voltages**: Full coverage + edge cases
- ✅ **Device Info**: Full coverage
- ✅ **Registers**: Full coverage
- ✅ **Manufacturer Access**: Full coverage
- ⏳ **Protection Status**: Pending validation
- ⏳ **Data Flash**: Partial coverage
- ⏳ **Chemistry Profiles**: Needs fixes
- ⏳ **FET Control**: Pending validation
- ⏳ **Cell Balancing**: Pending validation
- ⏳ **Custom Inputs**: Pending validation

## Experimental Features Testing

To test calibration wizard:
```bash
# Build with experimental features
cargo build --features experimental
cargo tauri dev --features experimental

# In UI, you should now see "Calibration Wizard [EXPERIMENTAL]" tab
```

Without experimental flag, the tab should be hidden.
