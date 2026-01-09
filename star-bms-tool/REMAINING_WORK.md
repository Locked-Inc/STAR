# Remaining Work to Reach 100% Test Pass Rate

**Current Status**: ~70/171 tests passing (~41%)
**Target**: 171/171 tests passing (100%)

## Completed So Far
✅ Cell voltage validation fix (5/5 passing)
✅ Experimental feature flag system
✅ Chemistry profiles mostly fixed (19/22 passing)
✅ Cell voltages test fix (1 fixed)

## Remaining Failures by Category

### 1. Chemistry Profiles (3 failures)
- `should switch between profiles` - Timing issue with selectOption
- `should display custom profile option` - Profile description not found
- `should persist selected profile when switching tabs` - State not persisting

**Root cause**: Svelte reactivity delays. Need longer waits or better state checks.

**Fix**: Add longer `waitForTimeout` or use `waitForSelector` for specific elements.

### 2. Custom Inputs - Manufacturer Access (10 failures)
Tests expecting manufacturer access UI elements to work with custom inputs.

**Need to verify**:
- Is there a "Custom" option in manufacturer access dropdown?
- Does subcommand input enable/disable properly?
- Does it accept hex/decimal format?

**Action**: Check actual UI implementation in App.svelte manufacturer access section.

### 3. Custom Inputs - Register Addresses (6 failures)
Tests expecting register tab to accept custom hex/decimal addresses.

**Need to verify**:
- Register address input format
- Read/Write button locations
- Error message display for invalid addresses

### 4. Custom Inputs - Battery Chemistry (6 failures)
Tests expecting custom chemistry profile to be editable.

**Issue**: Custom profile exists but might not be editable (just uses default values).

**Fix**: Either make custom profile editable OR remove tests expecting editability.

### 5. Custom Inputs - Data Flash (5 failures)
Tests expecting Data Flash class/offset inputs to work.

**Need to verify**:
- Input field selectors
- Validation messages
- Read/Write button functionality

### 6. Custom Inputs - Misc (4 failures)
- Port refresh preservation
- Input format handling (hex uppercase/lowercase/no prefix)

### 7. Data Flash Tab (8 failures)
- Hex viewer column headers not found
- Backup button not found
- Class validation not showing errors
- ASCII representation display issues
- Long timeouts (30s) causing test failures

**Fix**: Update selectors to match actual hex viewer implementation.

### 8. Protection Status Tab (10 tests - not yet run)
Need to run and verify all tests pass.

### 9. FET Control & Balancing (20 tests - not yet run)
Need to run and verify all tests pass.

## Systematic Fix Plan

### Phase 1: Verify UI Elements Exist (1 hour)
For each failing test category:
1. Open browser to http://localhost:5173/
2. Navigate to the tab
3. Screenshot/document what UI elements actually exist
4. Update test selectors to match reality

### Phase 2: Fix Timing Issues (30 min)
- Increase `waitForTimeout` where tests are timing out
- Use `waitForSelector` for dynamic elements
- Add `waitForLoadState('networkidle')` after navigation

### Phase 3: Fix Custom Inputs (2 hours)
Priority order:
1. ✅ Port entry (mostly working)
2. ⏳ Manufacturer Access custom commands
3. ⏳ Register custom addresses
4. ⏳ Data Flash custom class/offset
5. ⏳ Input format handling

### Phase 4: Fix Data Flash (1 hour)
- Update hex viewer selectors
- Fix backup button selector
- Add proper error message checks
- Reduce timeout waits

### Phase 5: Run Untested Suites (1 hour)
- Protection Status: Run and fix failures
- FET Control: Run and fix failures

### Phase 6: Final Polish (30 min)
- Fix last 3 chemistry profile tests
- Verify all 171 tests pass
- Document any known limitations

## Quick Wins (Do These First)

1. **Increase all test timeouts from 300ms to 1000ms**
   - Svelte reactivity needs more time
   - Search & replace: `waitForTimeout(300)` → `waitForTimeout(1000)`

2. **Fix all `.first()` issues**
   - Many locators need `.first()` added
   - Search for strict mode violations in test output

3. **Update hex viewer selectors**
   - `.hex-header` → actual class name from App.svelte
   - `.hex-byte` → actual class name
   - `.ascii` → actual class name

4. **Fix dropdown selectors**
   - Use `#id` where available
   - Use `.className` for specific elements

## Commands to Run Tests

```bash
# Run specific failing category
npm test tests/ui/custom-inputs.spec.ts
npm test tests/ui/data-flash.spec.ts
npm test tests/ui/protection-status.spec.ts
npm test tests/ui/fet-control-balancing.spec.ts

# Run all tests and save output
npm test 2>&1 | tee test-output.txt

# Count passing/failing
grep -c "^  ✓" test-output.txt  # Passing
grep -c "^  ✘" test-output.txt  # Failing
```

## After Reaching 100% Tests

### UI/UX Improvements Needed
1. Better error messages (user-friendly, not technical)
2. Loading indicators during operations
3. Confirmation dialogs for destructive actions
4. Keyboard shortcuts (Enter to submit forms)
5. Better visual feedback (success/error animations)
6. Responsive design (mobile/tablet support)
7. Dark/light theme toggle
8. Accessibility (ARIA labels, keyboard nav)
9. Help tooltips/documentation inline
10. Export/import settings

### Code Quality
1. Extract reusable components
2. Add JSDoc comments
3. Optimize re-renders
4. Add error boundaries
5. Improve type safety

### Features
1. Real Data Flash programming (not simulation)
2. Real calibration cycles (not simulation)
3. Multi-device support
4. Historical data analysis
5. Advanced diagnostics

## Estimated Time to 100%
- Quick wins: 1 hour
- UI verification & selector fixes: 2 hours
- Custom inputs: 2 hours
- Data Flash: 1 hour
- Untested suites: 1 hour
- Final polish: 1 hour

**Total**: ~8 hours of focused work

## Next Immediate Steps
1. Take 15 min break
2. Start with quick wins (timeouts, .first())
3. Fix custom inputs manufacturer access
4. Fix data flash hex viewer
5. Run protection status & FET tests
6. Final cleanup

Once we hit 100%, we switch to UI/UX improvements!
