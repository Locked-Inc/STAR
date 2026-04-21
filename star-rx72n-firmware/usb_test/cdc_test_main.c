/**
 * @file cdc_test_main.c
 * @brief Full-stack CDC bring-up harness using libs/rx_usb on Tom's RX72N board
 *
 * @details
 * Drop-in replacement for usb_test/main.c.  Instead of the hand-coded
 * usb_min.c bypass stack, this binary compiles the full libs/rx_usb CDC
 * composite stack (rx_usb.c + rx_usb_hw.c + rx_usb_isr.c + rx_usb_cdc.c)
 * and drives it from a ThreadX task so we can validate the 3-port CDC-ACM
 * composite end-to-end on Tom's board (HOCO -> PLL x12 -> UCK /4 = 48 MHz).
 *
 * Clock topology (Tom's board, no 24 MHz main oscillator fitted):
 *   - HOCO 16 MHz (+/-1 %) enabled in clock.c
 *   - PLL x12 -> 192 MHz
 *   - SCKCR2 UCK = /4 -> 48 MHz (USB spec +/-500 ppm; HOCO is +/-1 % so
 *     out-of-spec, works in practice because rx_usb_write() emits a short
 *     packet or ZLP on every message boundary)
 *
 * Workload:
 *   - Port 0 (k_usb_port_proto, /dev/ttyACM0)   : echo RX -> TX
 *   - Port 1 (k_usb_port_decoded, /dev/ttyACM1) : echo RX -> TX
 *   - Port 2 (k_usb_port_log, /dev/ttyACM2)     : periodic "tick=..." puts
 *
 * Heartbeat LEDs (shared with main.c):
 *   - PA7 -- firmware loop alive (toggles every 250 ms)
 *   - PB0 -- rx_usb_is_configured(k_usb_port_proto) == true
 *   - P71 -- any byte received on port 0 since boot
 *   - P72 -- rx_usb event callback has fired at least once
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_usb.h"
#include "tx_api.h"

extern void clock_init(void);
extern void cmt0_init(void);
extern void sci9_debug_init(void);
extern void sci9_debug_puts(const char* s);
extern void sci9_debug_puthex32(uint32_t v);

/** @brief Bumped on every USB0 USBI ISR entry.  Defined in rx_usb_isr.c. */
extern volatile uint32_t g_usb_isr_entry_count;

/* ==========================================================================
 * GPIO diagnostics (bit-banged, no dependency on rx_gpio)
 * ========================================================================== */

/**
 * @enum cdc_test_gpio_addrs_t
 * @brief Memory-mapped addresses for diagnostic LEDs on Tom's RX72N board
 */
typedef enum : uintptr_t {
  k_porta_pdr_addr  = 0x0008C00AU, /**< PORTA direction register */
  k_porta_podr_addr = 0x0008C02AU, /**< PORTA output data register */
  k_portb_pdr_addr  = 0x0008C00BU, /**< PORTB direction register */
  k_portb_podr_addr = 0x0008C02BU, /**< PORTB output data register */
  k_port7_pdr_addr  = 0x0008C007U, /**< PORT7 direction register */
  k_port7_podr_addr = 0x0008C027U, /**< PORT7 output data register */
} cdc_test_gpio_addrs_t;

/**
 * @enum cdc_test_gpio_bits_t
 * @brief Bit masks for the four diagnostic LEDs
 */
typedef enum : uint8_t {
  k_pa7_mask = (uint8_t)(1U << 7U), /**< PA7 heartbeat */
  k_pb0_mask = (uint8_t)(1U << 0U), /**< PB0 proto port configured */
  k_p71_mask = (uint8_t)(1U << 1U), /**< P71 any RX byte observed */
  k_p72_mask = (uint8_t)(1U << 2U), /**< P72 any USB event observed */
  k_pb5_mask = (uint8_t)(1U << 5U), /**< PB5 (pin 80) -- AD2 IO7 phase trace */
} cdc_test_gpio_bits_t;

static inline volatile uint8_t* porta_pdr(void) {
  return (volatile uint8_t*)k_porta_pdr_addr;
}
static inline volatile uint8_t* porta_podr(void) {
  return (volatile uint8_t*)k_porta_podr_addr;
}
static inline volatile uint8_t* portb_pdr(void) {
  return (volatile uint8_t*)k_portb_pdr_addr;
}
static inline volatile uint8_t* portb_podr(void) {
  return (volatile uint8_t*)k_portb_podr_addr;
}
static inline volatile uint8_t* port7_pdr(void) {
  return (volatile uint8_t*)k_port7_pdr_addr;
}
static inline volatile uint8_t* port7_podr(void) {
  return (volatile uint8_t*)k_port7_podr_addr;
}

/**
 * @brief Pulse PB5 (AD2 IO7) N times to mark a firmware phase
 *
 * @details
 * Each pulse is ~5 ms high, 5 ms low using busy loops sized for the LOCO
 * reset clock.  Safe to call at any clock speed because we just want a
 * sequence of edges the AD2 logic analyzer can count; exact width is not
 * important.
 *
 * @param[in] count Number of pulses to emit (1..15)
 */
static void pb5_mark_phase(uint8_t count) {
  *portb_pdr() |= k_pb5_mask;
  *portb_podr() &= (uint8_t)~k_pb5_mask;
  while (count != 0U) {
    *portb_podr() |= k_pb5_mask;
    for (volatile uint32_t d = 0U; d < 500U; d++) {
      __asm__ volatile("nop");
    }
    *portb_podr() &= (uint8_t)~k_pb5_mask;
    for (volatile uint32_t d = 0U; d < 500U; d++) {
      __asm__ volatile("nop");
    }
    count--;
  }
}

/**
 * @brief Slow variant of pb5_mark_phase with ~20 ms between pulses
 *
 * @details
 * Separates individual pulses widely enough that a 205 Hz AD2 capture
 * resolves each one as a distinct edge.  Pulse width itself stays narrow
 * (one sample) but the inter-pulse gap is ~20 ms so the logic analyser
 * can count them even in Single-mode with a 4096-sample buffer.
 *
 * Use for diagnostic readouts where the exact count matters (register
 * dumps, ISR entry counters).  Do NOT use inside time-critical paths.
 *
 * @param[in] count Number of pulses (0..15); 0 = silent marker
 */
static void pb5_mark_phase_slow(uint8_t count) {
  *portb_pdr() |= k_pb5_mask;
  *portb_podr() &= (uint8_t)~k_pb5_mask;
  /* Large leading/trailing silence so phase boundaries are unambiguous even
   * when the count is 0 (two back-to-back zero phases will still produce >1 s
   * of silence which survives the AD2 gap-grouping threshold). */
  tx_thread_sleep(200U);
  while (count != 0U) {
    *portb_podr() |= k_pb5_mask;
    tx_thread_sleep(15U);
    *portb_podr() &= (uint8_t)~k_pb5_mask;
    tx_thread_sleep(15U);
    count--;
  }
  tx_thread_sleep(200U);
}

/* ==========================================================================
 * USB event tracking (for LED diagnostics)
 * ========================================================================== */

/** @brief Incremented from rx_usb event callback (ISR context) */
static volatile uint32_t s_event_count;

/** @brief Incremented each time any port receives at least one byte */
static volatile uint32_t s_rx_count;

/**
 * @brief rx_usb event callback
 *
 * @details
 * Runs in USB0 USBI ISR context.  Keep short -- just bump counters so the
 * heartbeat task can show progress on P72.  Actual RX data handling is
 * done in the task loop via rx_usb_read().
 *
 * @param[in] port  Port that generated the event
 * @param[in] event Event type
 * @param[in] ctx   User context (unused)
 */
static void cdc_test_event_cb(rx_usb_port_id_t port, rx_usb_event_t event, void* ctx) {
  (void)port;
  (void)event;
  (void)ctx;
  s_event_count++;
}

/* ==========================================================================
 * ThreadX task config
 * ========================================================================== */

/**
 * @enum cdc_test_task_stack_t
 * @brief Stack size constants for the CDC test task
 */
typedef enum : uint16_t {
  k_cdc_task_stack_sz = 4096U, /**< Bytes of stack for cdc_test task */
} cdc_test_task_stack_t;

/**
 * @enum cdc_test_task_cfg_t
 * @brief Scheduling and loop timing constants
 */
typedef enum : uint8_t {
  k_cdc_task_priority  = 10U, /**< ThreadX priority (higher number = lower priority) */
  k_loop_period_ticks  = 5U,  /**< 5 ticks * 10 ms = 50 ms loop period */
  k_log_period_loops   = 20U, /**< Emit port-2 log line every 20 loops (~1 s) */
  k_rx_chunk_max_bytes = 64U, /**< RX buffer size per loop iteration */
} cdc_test_task_cfg_t;

static TX_THREAD s_cdc_tcb;
static uint8_t   s_cdc_stack[k_cdc_task_stack_sz];

/**
 * @brief Pump one port: read pending bytes, echo them back on the same port
 *
 * @details
 * Reads up to k_rx_chunk_max_bytes and writes them straight back.  The
 * BEMP handler in rx_usb_cdc_handle_bulk_in() will append a ZLP when the
 * last packet was exactly wMaxPacketSize so the host read(2) returns on
 * the message boundary.
 *
 * @param[in] port Port ID to pump (0 or 1 for R/W ports)
 *
 * @return true if at least one byte was echoed, false otherwise
 */
static bool cdc_test_pump_echo(rx_usb_port_id_t port) {
  uint32_t available = 0U;
  if (rx_usb_rx_available(port, &available) != k_rx_ok || available == 0U) {
    return false;
  }

  uint8_t        buf[k_rx_chunk_max_bytes];
  uint32_t       actual = 0U;
  const uint32_t want   = (available < (uint32_t)k_rx_chunk_max_bytes) ? available : (uint32_t)k_rx_chunk_max_bytes;
  if (rx_usb_read(port, buf, want, &actual) != k_rx_ok || actual == 0U) {
    return false;
  }
  (void)rx_usb_write(port, buf, actual);
  return true;
}

/**
 * @brief Main CDC test task
 *
 * @details
 * 1. Configures diagnostic LEDs and calls rx_usb_init() with our callback.
 * 2. Waits for the host to configure the device (port 0 Configured).
 * 3. Echoes RX -> TX on ports 0 and 1, periodically puts a log line on 2.
 *
 * @param[in] arg Unused
 */
static void cdc_test_task(ULONG arg) {
  (void)arg;

  *porta_pdr() |= k_pa7_mask;
  *portb_pdr() |= k_pb0_mask;
  *port7_pdr() |= (uint8_t)(k_p71_mask | k_p72_mask);
  *porta_podr() &= (uint8_t)~k_pa7_mask;
  *portb_podr() &= (uint8_t)~k_pb0_mask;
  *port7_podr() &= (uint8_t)~(k_p71_mask | k_p72_mask);

  pb5_mark_phase(3U); /* Phase 3: ThreadX task entered */
  pb5_mark_phase(4U); /* Phase 4: before rx_usb_init (sci9_puts skipped) */

  const rx_usb_config_t cfg = {
      .callback = cdc_test_event_cb,
      .ctx      = nullptr,
  };
  const rx_err_t init_err = rx_usb_init(&cfg);
  pb5_mark_phase(5U); /* Phase 5: rx_usb_init returned */
  /* Encode init_err on PB5: after phase 5, burst 10 pulses if error, 11 if ok. */
  pb5_mark_phase((init_err == k_rx_ok) ? 11U : 10U);

  /* Post-init diagnostic bursts -- PB5 pulses encode HW register state so we
   * can verify on the AD2 logic analyser that the ISR-enable path actually
   * wrote the expected bits.  Format: each value gets a 15-pulse marker
   * separator, then a burst whose pulse count equals the value we care about.
   */
  {
    /* Absolute register addresses (avoid depending on rx_hal headers for
     * this debug-only block).  Verified against Renesas FSP iodefine.h
     * via compile-time probe.  USBI0 lives on SELECTB slot 144 (not
     * fixed vec 36).
     *   IER[18]   = 0x00087200 + 18       = 0x00087212, bit 0 (144 % 8 == 0)
     *   IPR[144]  = 0x00087300 + 144      = 0x00087390  (144 dec = 0x90)
     *   SLIBR[144]= 0x00087700 + 144      = 0x00087790  (must read 62 = USBI0) */
    volatile uint16_t* const usb0_syscfg   = (volatile uint16_t*)0x000A0000U;
    volatile uint16_t* const usb0_intenb0  = (volatile uint16_t*)0x000A0030U;
    volatile uint8_t* const  icu_ier18     = (volatile uint8_t*)0x00087212U;
    volatile uint8_t* const  icu_ipr144    = (volatile uint8_t*)0x00087390U;
    volatile uint8_t* const  icu_slibr144  = (volatile uint8_t*)0x00087790U;

    const uint16_t intenb0    = *usb0_intenb0;
    const uint16_t syscfg     = *usb0_syscfg;
    const uint8_t  ier18      = *icu_ier18;
    const uint8_t  ipr144     = *icu_ipr144;
    const uint8_t  slibr144   = *icu_slibr144;

    /* popcount of the five enable bits we set (VBSE|DVSE|CTRE|BRDYE|BEMPE).
     * Expected = 5 if internal_usb_configure_interrupts() ran correctly. */
    uint8_t intenb_pop = 0U;
    if ((intenb0 & (uint16_t)(1U << 15U)) != 0U) intenb_pop++; /* VBSE */
    if ((intenb0 & (uint16_t)(1U << 12U)) != 0U) intenb_pop++; /* DVSE */
    if ((intenb0 & (uint16_t)(1U << 11U)) != 0U) intenb_pop++; /* CTRE */
    if ((intenb0 & (uint16_t)(1U << 10U)) != 0U) intenb_pop++; /* BEMPE */
    if ((intenb0 & (uint16_t)(1U <<  8U)) != 0U) intenb_pop++; /* BRDYE */

    /* USBE|SCKE|DPRPU -- expected 3 after attach */
    uint8_t syscfg_pop = 0U;
    if ((syscfg & (uint16_t)(1U <<  0U)) != 0U) syscfg_pop++; /* USBE */
    if ((syscfg & (uint16_t)(1U <<  4U)) != 0U) syscfg_pop++; /* DPRPU */
    if ((syscfg & (uint16_t)(1U << 10U)) != 0U) syscfg_pop++; /* SCKE */

    /* Every value is emitted as (value + 1) so no phase collapses into
     * silence when the register reads as 0.  Decode: observed - 1. */
    pb5_mark_phase_slow(15U);                  /* marker: init diag start */
    pb5_mark_phase_slow(intenb_pop + 1U);      /* expect 6 pulses (5 bits set) */
    pb5_mark_phase_slow(13U);                  /* divider */
    pb5_mark_phase_slow((uint8_t)(((ier18 & 0x01U) != 0U ? 1U : 0U) + 1U));
    pb5_mark_phase_slow(13U);                  /* divider */
    pb5_mark_phase_slow((uint8_t)((ipr144 & 0x0FU) + 1U)); /* expect 7 pulses (IPR=6) */
    pb5_mark_phase_slow(13U);                  /* divider */
    pb5_mark_phase_slow((uint8_t)((slibr144 == 62U ? 1U : 0U) + 1U)); /* 2 pulses iff SLIBR=62 */
    pb5_mark_phase_slow(13U);                  /* divider */
    pb5_mark_phase_slow((uint8_t)(syscfg_pop + 1U)); /* expect 4 pulses (USBE+DPRPU+SCKE) */
    pb5_mark_phase_slow(15U);                  /* marker: init diag end */
  }

  uint32_t tick  = 0U;
  uint32_t loops = 0U;
  uint32_t isr_report_loops = 0U;
  bool     isr_report_emitted = false;
  for (;;) {
    *porta_podr() ^= k_pa7_mask;
    /* PB5 reserved for diagnostic bursts -- do NOT toggle from task loop. */

    isr_report_loops++;
    if (!isr_report_emitted && isr_report_loops >= 500U) {  /* emit ONE report after ~settle */
      isr_report_emitted = true;

      /* Sample IR[144] and INTSTS0 without disturbing them.  IR[144]
       * = ICU base (0x00087000) + IR offset (0x000) + 144 = 0x00087090. */
      volatile uint8_t* const  icu_ir144     = (volatile uint8_t*)0x00087090U;
      volatile uint16_t* const usb0_intsts0  = (volatile uint16_t*)0x000A0040U;
      const uint8_t  ir144   = *icu_ir144;
      const uint16_t intsts0 = *usb0_intsts0;

      uint8_t intsts_pop = 0U;
      if ((intsts0 & (uint16_t)(1U << 15U)) != 0U) intsts_pop++; /* VBINT */
      if ((intsts0 & (uint16_t)(1U << 14U)) != 0U) intsts_pop++; /* RESM */
      if ((intsts0 & (uint16_t)(1U << 13U)) != 0U) intsts_pop++; /* SOFR */
      if ((intsts0 & (uint16_t)(1U << 12U)) != 0U) intsts_pop++; /* DVST */
      if ((intsts0 & (uint16_t)(1U << 11U)) != 0U) intsts_pop++; /* CTRT */
      if ((intsts0 & (uint16_t)(1U << 10U)) != 0U) intsts_pop++; /* BEMP */
      if ((intsts0 & (uint16_t)(1U <<  9U)) != 0U) intsts_pop++; /* NRDY */
      if ((intsts0 & (uint16_t)(1U <<  8U)) != 0U) intsts_pop++; /* BRDY */
      if ((intsts0 & (uint16_t)(1U <<  7U)) != 0U) intsts_pop++; /* VBSTS */
      if ((intsts0 & (uint16_t)(1U <<  3U)) != 0U) intsts_pop++; /* VALID */

      /* Encoded as (value + 1) so every phase emits >= 1 pulse.  This
       * prevents a 0 value from collapsing silently into neighboring silence
       * (which happened at the tick rates we are seeing).  Decode: observed_pulses - 1. */
      pb5_mark_phase_slow(14U);  /* marker: ISR report start */
      uint32_t cnt = g_usb_isr_entry_count;
      if (cnt > 9U) {
        cnt = 9U;
      }
      pb5_mark_phase_slow((uint8_t)(cnt + 1U));              /* ISR count + 1 */
      pb5_mark_phase_slow(13U);                              /* divider */
      pb5_mark_phase_slow((uint8_t)((ir144 & 0x01U) + 1U));  /* IR[144] + 1 */
      pb5_mark_phase_slow(13U);                              /* divider */
      pb5_mark_phase_slow((uint8_t)(intsts_pop + 1U));       /* INTSTS0 popcount + 1 */
      pb5_mark_phase_slow(14U);  /* marker: ISR report end */
      /* Big inter-report silence: absolutely nothing should appear on PB5
       * between reports, unless the ISR fires (nanosecond pulse, invisible
       * at current AD2 sample rate, but g_usb_isr_entry_count will catch it). */
      tx_thread_sleep(800U);
    }

    if (rx_usb_is_configured(k_usb_port_proto)) {
      *portb_podr() |= k_pb0_mask;
    }
    if (s_event_count > 0U) {
      *port7_podr() |= k_p72_mask;
    }

    if (rx_usb_is_configured(k_usb_port_proto)) {
      if (cdc_test_pump_echo(k_usb_port_proto)) {
        s_rx_count++;
      }
    }
    if (rx_usb_is_configured(k_usb_port_decoded)) {
      if (cdc_test_pump_echo(k_usb_port_decoded)) {
        s_rx_count++;
      }
    }
    if (s_rx_count > 0U) {
      *port7_podr() |= k_p71_mask;
    }

    loops++;
    if (loops >= (uint32_t)k_log_period_loops) {
      loops = 0U;
      tick++;
      if (rx_usb_is_configured(k_usb_port_log)) {
        char     line[32];
        uint8_t  i      = 0U;
        line[i++]       = 't';
        line[i++]       = 'i';
        line[i++]       = 'c';
        line[i++]       = 'k';
        line[i++]       = '=';
        uint32_t       t = tick;
        char           tmp[10];
        uint8_t        tmp_i = 0U;
        do {
          tmp[tmp_i++] = (char)('0' + (t % 10U));
          t /= 10U;
        } while (t != 0U);
        while (tmp_i > 0U) {
          tmp_i--;
          line[i++] = tmp[tmp_i];
        }
        line[i++] = '\r';
        line[i++] = '\n';
        (void)rx_usb_write(k_usb_port_log, (const uint8_t*)line, (uint32_t)i);
      }
    }

    tx_thread_sleep((ULONG)k_loop_period_ticks);
  }
}

/**
 * @brief ThreadX application init -- creates the single CDC test task
 *
 * @param[in] first_unused_memory Unused
 */
void tx_application_define(void* first_unused_memory) {
  (void)first_unused_memory;

  (void)tx_thread_create(&s_cdc_tcb,
                         "cdc_test",
                         cdc_test_task,
                         0U,
                         s_cdc_stack,
                         (ULONG)k_cdc_task_stack_sz,
                         k_cdc_task_priority,
                         k_cdc_task_priority,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);
}

/**
 * @brief Quick visual heartbeat: flash PA7 N times at LOCO speed
 *
 * @details
 * Runs before clock_init() so it uses the 240 kHz LOCO reset clock.
 * Useful to confirm the firmware is actually executing before PLL is up.
 *
 * @param[in] count Number of blinks
 */
static void early_blink(uint8_t count) {
  *porta_pdr() |= k_pa7_mask;
  *porta_podr() &= (uint8_t)~k_pa7_mask;
  for (uint8_t n = 0U; n < count; n++) {
    *porta_podr() |= k_pa7_mask;
    for (volatile uint32_t d = 0U; d < 20000U; d++) {
      __asm__ volatile("nop");
    }
    *porta_podr() &= (uint8_t)~k_pa7_mask;
    for (volatile uint32_t d = 0U; d < 20000U; d++) {
      __asm__ volatile("nop");
    }
  }
}

int main(void) {
  pb5_mark_phase(7U);
  clock_init();
  pb5_mark_phase(1U);
  cmt0_init();
  pb5_mark_phase(2U);
  sci9_debug_init();
  tx_kernel_enter();
  /* never returns */
  for (;;) {
  }
}
