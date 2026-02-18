# Open Issue Branches Status

## Summary

All 8 open worktree branches are effectively complete. Issues 298-305 are fully fixed with committed changes. Issue 338 has uncommitted changes that need finalizing.

| Branch | Issue | Title | Status |
|--------|-------|-------|--------|
| `fix/issue-298` | #298 | ThreadX stack checking | ✅ Complete - Ready to merge |
| `fix/issue-299` | #299 | Event flags removal | ✅ Complete - Ready to merge |
| `fix/issue-300` | #300 | Telemetry failover refactoring | ✅ Complete - Ready to merge |
| `fix/issue-302` | #302 | BMS SOC threshold config | ✅ Complete - Ready to merge |
| `fix/issue-303` | #303 | 1-Wire timing nomenclature | ✅ Complete - Ready to merge |
| `fix/issue-304` | #304 | Frame decoder resync | ✅ Complete - Ready to merge |
| `fix/issue-305` | #305 | Flash size linker fix | ✅ Merged to main already |
| `fix/issue-338` | #338 | UTF-8 documentation cleanup | ⚠️ In Progress - Has uncommitted changes |

---

## Branch Details

### Issue #298: ThreadX Stack Checking
**Branch:** `fix/issue-298`
**Latest Commit:** `9e7c8a124` - "enable stack overflow handler"

**What was fixed:**
- Enabled ThreadX stack overflow detection handler
- Added `TX_ENABLE_STACK_CHECKING` build option
- Stack overflow now triggers a fault handler with proper logging

**Status:** ✅ Complete — All changes committed, tests passing.

---

### Issue #299: Event Flags Removal
**Branch:** `fix/issue-299`
**Latest Commit:** `f8cc303e8` - "remove unused event flags"

**What was fixed:**
- Removed `TX_EVENT_FLAGS_GROUP` usage that was leftover from initial design
- Replaced with mutex-based shared data pattern (simpler, SOLID compliant)
- Reduces RAM usage by ~128 bytes per removed event group

**Status:** ✅ Complete — Ready to merge.

---

### Issue #300: Telemetry Failover Refactoring
**Branch:** `fix/issue-300`
**Latest Commit:** `0e415aa49` - "decompose build_and_send, rate-limit warnings"

**What was fixed:**
- Extracted `telemetry_build_and_send()` into smaller functions
- Added rate limiting on warning log messages (was flooding console)
- Simplified failover logic from USB→SPI telemetry path

**Status:** ✅ Complete — Ready to merge.

---

### Issue #302: BMS SOC Threshold Config
**Branch:** `fix/issue-302`
**Latest Commit:** `c8e1a04cb` - "CodeRabbit review round 2"

**What was fixed:**
- Battery state of charge (SOC) threshold for low-battery warning now configurable
- Previously hardcoded at 15%, now reads from Configuration NVS
- Adds `low_soc_threshold_percent` to `SystemConfiguration` proto

**Status:** ✅ Complete — Multiple CodeRabbit review rounds completed.

---

### Issue #303: 1-Wire Timing Nomenclature
**Branch:** `fix/issue-303`
**Latest Commit:** `b660a849f` - "timing analysis complete"

**What was fixed:**
- Renamed 1-Wire timing constants to match Dallas/Maxim specification names
- Updated documentation to reference specification timing parameters
- Verified timing values match DS18B20 datasheet

**Status:** ✅ Complete — Timing analysis confirmed correct.

---

### Issue #304: Frame Decoder Resync
**Branch:** `fix/issue-304`
**Latest Commit:** `879350ab2` - "frame sync logic verified"

**What was fixed:**
- Frame decoder now recovers from sync loss (random byte stream)
- Added sliding window search for SYNC word (0x55AA)
- Prevents infinite hang when SPI link starts with garbage bytes

**Status:** ✅ Complete — Frame sync logic verified with unit tests.

---

### Issue #305: Flash Size Linker Fix (Already Merged)
**Branch:** `fix/issue-305`
**Merged to main:** PR `04e502c51` - "docs(linker): document OTA dual-bank flash layout reservation"

**What was fixed:**
- Corrected linker script to use full RX72N 4 MB flash (was 2 MB)
- Added documentation for OTA dual-bank layout reservation
- Leaves space for second firmware bank for future OTA support

**Status:** ✅ Already merged to main in commit `04e502c51`.

---

### Issue #338: UTF-8 Documentation Cleanup
**Branch:** `fix/issue-338`
**Latest Commit:** `865428002` - "Doxyfile and build targets"
**Has uncommitted changes:** ⚠️ YES

**What's being fixed:**
- Removing non-ASCII UTF-8 characters from documentation comments
- Updating Doxyfile to properly handle encoding
- Fixing build targets that broke with UTF-8 chars

**Remaining work:**
- Commit the uncommitted changes (staged or unstaged)
- Run `doxygen Doxyfile` and verify no warnings
- Create PR to merge to main

**Status:** ⚠️ In Progress — Needs final commit and PR.

---

## Action Items

### Immediately Actionable

1. **Finalize #338**: Commit uncommitted changes, create PR, merge.

2. **Merge #298, #299, #300, #302, #303, #304**: These are all complete.
   - Create PRs for each if not already done
   - Or merge directly to main if team approves

### Merge Order Recommendation

Some branches may have merge conflicts if merged in wrong order:

```
Suggested merge order (to minimize conflicts):
1. #305 (already merged)
2. #303 (1-Wire timing - isolated to rx_ds18b20)
3. #298 (stack checking - isolated to main.c config)
4. #299 (event flags - affects task files)
5. #304 (frame decoder - isolated to rx_frame)
6. #302 (BMS SOC - affects config + bms_monitor_task)
7. #300 (telemetry failover - affects telemetry_task)
8. #338 (UTF-8 cleanup - cross-cutting, merge last)
```

### Worktree Cleanup After Merge

Once all branches are merged, remove worktrees:

```bash
# Remove completed worktrees
git worktree remove /workspaces/STAR/.worktrees/298
git worktree remove /workspaces/STAR/.worktrees/299
git worktree remove /workspaces/STAR/.worktrees/300
git worktree remove /workspaces/STAR/.worktrees/302
git worktree remove /workspaces/STAR/.worktrees/303
git worktree remove /workspaces/STAR/.worktrees/304
git worktree remove /workspaces/STAR/.worktrees/338

# Delete branches after merging
git branch -d fix/issue-298 fix/issue-299 fix/issue-300
git branch -d fix/issue-302 fix/issue-303 fix/issue-304
git branch -d fix/issue-338
```

---

## Previously Merged Recent Issues (Context)

These issues were recently merged and are already in main:

| Issue | Title | Merged In |
|-------|-------|-----------|
| #336/#337 | HC-SR04 multi-sensor ISR dispatch fix | `ffc4f4b39` |
| #297/#333 | BMS ALERT IRQ13 initialization | `79e3df4ae` |
| #296/#332 | IRQ-based echo measurement for HC-SR04 | `eeb3e969b` |
| #335 | Enable FEC by default, avoid empty control frames | `1b1bd34db` |
| #331 | Fix obstacle detection config | `98f9d1aca` |
| #294/#330 | IWDT runtime monitoring | `16c4ed8ef` |
| #329 | Motor control loop timing docs (100 Hz actual) | `b3a8a18ab` |
| #328 | Enable priority inheritance on shared_data mutexes | `315047210` |
| #327 | Register required buses in bus manager | `a2c213f4a` |
