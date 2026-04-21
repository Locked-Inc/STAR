Add RX72N Low Power Consumption HAL: sleep, all-module clock stop,
software standby, deep software standby (DSBY), and the matching
wake-up source plumbing.

Files (create -- none of these exist yet):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_lpc_regs.h
  (rename if rx72n_lpc_regs.h already exists -- there's a stub on main;
  audit it and EXTEND it with the registers we don't yet cover, don't
  duplicate)
- star-rx72n-firmware/libs/rx_hal/inc/rx_lpc.h
- star-rx72n-firmware/libs/rx_hal/src/rx_lpc.c
- star-rx72n-firmware/tests/test_rx_lpc.c
- star-rx72n-firmware/tests/mocks/mock_rx_lpc.h
- star-rx72n-firmware/tests/mocks/mock_rx_lpc.c

Source-of-truth policy:
- Renesas RX72N HW manual is THE authoritative source.
  docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf chapter 12
  "Low Power Consumption" (verify chapter number against the table
  of contents -- the LPC chapter location varies by manual revision).
  Every constant in our generated code must cite a manual page or
  section number in the doc-comment.
- /tmp/hirakuni45-RX/RX600/system.hpp (SBYCR around line 919, DPSBYCR
  around line 1157, DPSBY bit @ B7) plus /tmp/hirakuni45-RX/RX72N/{
  peripheral.hpp, icu.hpp (NMI / IRQ wake-up vectors),
  power_mgr.hpp } is a CROSS-REFERENCE ONLY -- use it to find what to
  look up in the manual, then cite the manual page in source comments.
  Do NOT cite hirakuni45 in source comments, file headers, or commit
  messages. Treat it like a peer's notes -- helpful for navigation,
  never the citation. If hirakuni45 and the manual disagree, the
  manual wins, and flag the disagreement in the commit body so a
  human can adjudicate.

Use case for STAR: the robot is normally always-on, but bench-test
firmware variants (the encoder_handspin / uart_test / blinky probes,
and any future field-deployable battery monitor) want to drop into
sleep between samples instead of busy-waiting. Three concrete cases
this driver enables:

  1. Sleep mode -- CPU stops on `WAIT`, peripherals keep running.
     ThreadX `tx_thread_sleep` ultimately wants this for the idle
     thread on battery-aware builds.
  2. Software standby -- almost everything stops, ~3 uA. Wake on NMI,
     IRQn pin, or the special standby-aware peripherals (RTC, IWDT,
     LVD). Useful for "park the robot at a charging dock and wait for
     the user to press the button" deployments.
  3. Deep software standby (DSBY) -- entire chip off except the
     deep-standby IRQ block + RTC + LVD. Lowest power. Wake-up
     restarts execution from the reset vector with the DPSBY flag
     set so we can distinguish a deep-standby wake from a cold boot.

The current RX72N firmware never enters any of these modes, so this
driver is greenfield. We need it BEFORE we can credibly claim a
battery budget for the robot.

Implement:
1. rx72n_lpc_regs.h: register struct(s) following the pattern of every
   other rx72n_*_regs.h in the repo (typed enums for addresses, packed
   volatile struct of registers, bit-field constants for each register).
   Register coverage MUST include at minimum:
     SBYCR    (Standby Control Register, @0x0008000C, 16-bit) -- SSBY
              bit selects sleep vs software standby.
     MSTPCRA, MSTPCRB, MSTPCRC, MSTPCRD -- already partly defined in
              rx72n_system_regs.h; reference, do not duplicate.
     DPSBYCR  (Deep Standby Control Register, @0x0008C280) -- DPSBY,
              IOKEEP, DEEPCUT[1:0].
     DPSIER0..3 (Deep Standby Interrupt Enable) and DPSIFR0..3 (flags)
              and DPSIEGR0..3 (edge select) -- which IRQs can wake from
              deep standby.
     OPCCR    (Operating Power Control Register, @0x000800A0) -- selects
              high-speed vs middle-speed vs low-speed mode (this is
              power vs performance, not sleep depth, but it lives in
              the same chapter and the driver should expose it).
     SOPCCR   (Sub-Operating Power Control Register).
   For each: verify base address, field offset, bit position, mask
   against the manual.
2. rx_lpc.h: clean public API. Wake-up source enum:
     k_lpc_wake_nmi
     k_lpc_wake_irq0..k_lpc_wake_irq15
     k_lpc_wake_lvd1, k_lpc_wake_lvd2
     k_lpc_wake_rtc_alarm, k_lpc_wake_rtc_periodic
     k_lpc_wake_iwdt_underflow
     k_lpc_wake_usb0_resume
   API:
     rx_err_t rx_lpc_init(void);
     rx_err_t rx_lpc_set_operating_power(rx_lpc_opcc_mode_t mode);
       /* k_lpc_opcc_high_speed (default) | k_lpc_opcc_middle_speed |
          k_lpc_opcc_low_speed | k_lpc_opcc_subosc_speed */
     rx_err_t rx_lpc_enter_sleep(void);
       /* CPU only; peripherals keep running; WAIT instruction */
     rx_err_t rx_lpc_enter_software_standby(uint32_t wake_mask);
       /* SSBY=1, then WAIT; resumes here after wake-up */
     rx_err_t rx_lpc_enter_deep_software_standby(uint32_t wake_mask,
                                                 bool keep_io_state);
       /* DPSBY=1; resumes at reset vector. keep_io_state controls
          DPSBYCR.IOKEEP so peripheral pins stay in their last state
          across the deep-standby cycle. */
     bool     rx_lpc_was_deep_standby_wake(void);
       /* Read DPSBYCR.DPSBY at boot to tell DSBY-wake from cold reset. */
     uint32_t rx_lpc_get_wake_flags(void);
       /* Read + clear DPSIFR0..3 so the caller knows what woke us. */
3. rx_lpc.c: driver. Must
     - Configure SBYCR.SSBY appropriately for each entry path.
     - For software standby and DSBY, set the corresponding wake-up
       enable bits in DPSIER0..3 (and edge selects via DPSIEGR0..3)
       BEFORE issuing WAIT. Mask everything else.
     - Issue WAIT as inline assembly (the only place in the codebase
       __asm__ is appropriate -- document why per CLAUDE.md "Critical
       Rules"). Pre-condition: no pending unmasked IRQs.
     - For DSBY: write DPSBYCR.DPSBY = 1 immediately before WAIT.
       Manual section 12.x says you must clear all DPSIFR flags first
       and that any flag set after that point will block entry --
       respect both.
     - Follow rx_register_protection.h for PRCR sequencing if any
       register is PRCR-protected (some are, manual will tell you).
     - Use rx_log_error / rx_log_info for diagnostics. Note that log
       output via SCI may be lost if SCI is in module-stop -- driver
       must NOT log between "preparing standby" and "WAIT" inside the
       enter-standby paths.
     - NASA Power of 10 compliant (>=2 pre/post conditions per fn,
       no goto/recursion, return-value check on every call).
4. tests/test_rx_lpc.c + mocks: same shape as test_rx_iwdt.c +
   mocks/mock_rx_iwdt.{h,c}. Cover:
     - init / opcc-mode happy path + invalid mode rejection
     - sleep entry: WAIT replaced by mock-callable in the mock so the
       host test isn't actually halted
     - software standby entry: verify SBYCR.SSBY is set and DPSIER
       wake mask is programmed
     - DSBY entry: verify DPSBY+IOKEEP, all DPSIFR cleared first,
       wake mask programmed
     - was_deep_standby_wake() reads DPSBY correctly across boot
     - get_wake_flags() returns and clears DPSIFR
5. Wire test_rx_lpc into tests/CMakeLists.txt next to test_rx_iwdt.

Coding style -- MUST follow /workspaces/STAR/CLAUDE.md. The rules that
will absolutely come up while writing this peripheral:

- All headers use `#pragma once` (no traditional include guards).
- All files pure 7-bit ASCII (no em-dashes, no Unicode tree drawings,
  no smart quotes). The pre-commit hook + CI ASCII Check will reject
  the commit otherwise. Use `--` for em-dashes and `|--` `` `-- `` for
  tree drawings if any.
- Naming:  functions/vars `snake_case`,  types `snake_case_t`,
  static/file-scope vars `s_<name>`,  static (testable) functions
  `internal_<name>`,  private helpers `priv_<name>`,  globals avoided
  (and `g_` if unavoidable).
- ZERO magic numbers. Every numeric literal -- including bit shifts,
  array indices, register offsets, address constants -- MUST be a C23
  typed enum with an explicit underlying type:
      typedef enum : uint8_t  { ... } foo_t;
      typedef enum : uintptr_t{ k_xxx_base = 0x000800XX, ... } addresses_t;
  Use `uintptr_t` for register-base-address enums (NOT uint32_t -- it
  silently truncates on the 64-bit unit-test host). uint8_t for shifts
  and small constants, uint16_t for medium, uint32_t for masks.
- Hardware register access via inline accessors that return
  `volatile <type> *` (NOT macros) -- see existing mtu1() / tpu_control()
  / system_regs() / prcr_reg() in libs/rx_hal/inc/. Add new accessors
  for the LPC registers you add.
- Inline `__asm__("WAIT")` is allowed here (it's the only way to enter
  sleep / standby modes); document why with a `@par Inline ASM:` block
  in the function's Doxygen and add `// NOLINT(hicpp-no-assembler)` if
  clang-tidy complains.
- NASA Power of 10:
    1. No goto / setjmp / recursion.
    2. All loops have static upper bounds (use the typed enum).
    3. No dynamic allocation after init -- everything static.
    4. Functions ~60 lines max (clang-tidy CI fails at 60+).
    5. At least 2 pre-conditions and 2 post-conditions per function
       (use RX_CHECK_NULL_PTR / RX_VALIDATE_PTR / RX_VALIDATE_INIT).
    7. Every function-call return value either checked or explicitly
       cast to (void).  Use RX_RETURN_ON_ERROR.
    8. Macros only for: code-deduplication (RX_RETURN_ON_ERROR style),
       conditional compilation, build-config flags. NOT for register
       addresses -- use accessors.
    9. Function pointers ONLY for the dependency-injection interface
       struct pattern (test mock injection); document why with @par.
   10. -Wall -Wextra -Werror; CI fails on any warning.
- Doxygen on EVERY file, function, struct, enum, variable, typedef,
  and macro. Use ALL applicable tags. Reference rx_pid_compute() in
  libs/rx_pid/src/rx_pid.c as the canonical example. Minimum per fn:
  @brief, @details, @param[in/out], @return, @retval (every value),
  @pre (>=2), @post (>=2), @note (thread safety), @see, @since.
  For the standby-entry functions, ALSO include a @warning that lists
  exactly which subsystems lose state (peripheral clocks, RAM regions
  if not retained, MTU TCNT counters, etc.) per the manual table.
- Inclusive terminology: Controller / Peripheral, COPI / CIPO, CS,
  Primary / Main. NOT master / slave / MOSI / MISO / SS.
- No backward-compatibility shims, no deprecation aliases, no
  function-name `#define`s -- per CLAUDE.md "Backward Compatibility
  Policy": zero compat layers, breaking changes encouraged.
- Tests follow the same style and live under tests/. Mocks under
  tests/mocks/. Match the exact shape of existing test_rx_iwdt.c +
  mocks/mock_rx_iwdt.{h,c}.
- Run `python3 scripts/utils/fix-encoding.py --check <file>` on every
  new file before committing to confirm ASCII-only.
- Run clang-format-15 on every new .c/.h before committing.

After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"
  bash scripts/hal-audit/devcontainer-exec.sh \
      "cd star-rx72n-firmware/tests && rm -rf build && cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure"

Both must pass. If either fails, do not commit; print the failure.

If both pass:
  git add -A
  git commit -m "lib(lpc): add Low Power Consumption (sleep / standby / deep standby) driver per RX72N HW manual"
  git push origin bsikar/verifying-low-power

Output:
  OK  <file>:<line> <const>=<value>  (manual page <N>)
  NEW <file>:<line> <const>=<value>  (manual page <N>)

No wall-time cap. Take the time you need to do this right -- standby
modes have subtle entry/exit ordering requirements (clear DPSIFR
before setting DPSBY, no SCI logging between SBYCR write and WAIT,
which IRQs can/cannot wake from each mode), and getting them wrong
turns into "robot bricks itself in deep standby with no wake source"
which is a hard-to-debug class of bug.
