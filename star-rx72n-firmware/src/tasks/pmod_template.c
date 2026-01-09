/* src/tasks/pmod_template.c */

/**
 * @file pmod_template.c
 * @brief PMOD Task Template Implementation
 *
 * This is a comprehensive template for creating PMOD expansion module tasks.
 * It demonstrates three common PMOD interfaces: GPIO, I2C, and SPI.
 *
 * IMPORTANT: This file is a TEMPLATE ONLY and should NOT be compiled directly.
 * To create a new PMOD module:
 * 1. Copy this file to pmod_<module>.c
 * 2. Copy pmod_template.h to pmod_<module>.h
 * 3. Find/replace "template" with your module name
 * 4. Remove unused interface examples (keep only GPIO/I2C/SPI as needed)
 * 5. Implement hardware-specific initialization and logic
 * 6. Configure in pmod_config.h (set type, priority, stack size)
 * 7. Add to CMakeLists.txt
 * 8. Call pmod_<module>_create() from tx_application_define()
 *
 * Priority Guidelines:
 * - 10-15: High-rate sensors (>50 Hz), real-time communication
 * - 16-23: Medium-rate sensors and I/O (1-50 Hz)
 * - 24-31: Low-rate background tasks (<1 Hz)
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

/*
 * IMPORTANT: Comment out this entire file or exclude from build.
 * This is a TEMPLATE ONLY - do not compile directly!
 */
#if 0

#include "tasks/pmod_template.h"

#include <string.h>

#include "pmod_config.h"
#include "system_config.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "shared_state.h"

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* Module name for logging */
static char* s_tag = "pmod_template";

/* ThreadX thread control block and stack */
static TX_THREAD s_pmod_template_thread;
static uint8_t s_pmod_template_stack[2048]; /* Adjust size as needed */

/* Thread priority (10-31 for PMOD tasks) */
typedef enum {
  k_priority_pmod_template = 20, /**< Medium priority (configurable) */
} pmod_template_priority_t;

/* Update rate (Hz) */
typedef enum {
  k_update_rate_hz = 10, /**< 10 Hz = 100ms period */
} pmod_template_rate_t;

/* =============================================================================
 * Module State (Static Allocation)
 * =============================================================================
 */

/**
 * @brief PMOD template module state
 *
 * Example state structure - customize for your module.
 * All state must be statically allocated (no malloc/free).
 */
typedef struct {
  uint32_t sample_count;    /**< Total samples collected */
  float last_value;         /**< Last measured value */
  bool initialized;         /**< Module initialized flag */
  bool fault;               /**< Fault detected flag */
} pmod_template_state_t;

/* Module state (static allocation) */
static pmod_template_state_t s_state = {0};

/* =============================================================================
 * Hardware Interface Examples
 * =============================================================================
 */

/**
 * @brief Initialize GPIO-based PMOD module
 *
 * Example: PmodLED (4 LEDs), PmodBTN (4 buttons), PmodSWT (4 switches)
 *
 * GPIO PMOD Pinout (6-pin):
 * - Pin 1-4: GPIO (configurable input/output)
 * - Pin 5:   GND
 * - Pin 6:   VCC (3.3V)
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t init_gpio_module(void)
{
  /* Example: Initialize GPIO pins for LED output
   *
   * Note: Configure actual GPIO pins from hardware_pinout.h
   * Example:
   *   gpio_set_output(k_port_b, 0);  // LED 0
   *   gpio_set_output(k_port_b, 1);  // LED 1
   *   gpio_set_output(k_port_b, 2);  // LED 2
   *   gpio_set_output(k_port_b, 3);  // LED 3
   */

  rx_log_info(s_tag, "GPIO module initialized");
  return k_rx_ok;
}

/**
 * @brief Initialize I2C-based PMOD module
 *
 * Example: PmodACL (accelerometer), PmodTMP2 (temperature), PmodALS (light)
 *
 * I2C PMOD Pinout (8-pin):
 * - Pin 1:   SCL (I2C clock)
 * - Pin 2:   SDA (I2C data)
 * - Pin 3:   INT (interrupt, optional)
 * - Pin 4:   RST (reset, optional)
 * - Pin 5-6: GND
 * - Pin 7-8: VCC (3.3V)
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t init_i2c_module(void)
{
  /* Example: Initialize I2C bus for sensor
   *
   * Note: Use rx_bus_manager with I2C interface
   * Example:
   *   rx_bus_manager_t bus_mgr = {0};
   *   rx_bus_config_t config = {
   *     .bus_type = k_rx_bus_i2c,
   *     .i2c.channel = k_rx_riic_channel_0,
   *     .i2c.address = 0x1D,  // Device I2C address
   *     .i2c.clock_hz = 100000, // 100 kHz standard mode
   *   };
   *   rx_err_t ret = rx_bus_manager_init(&bus_mgr, &config);
   */

  rx_log_info(s_tag, "I2C module initialized");
  return k_rx_ok;
}

/**
 * @brief Initialize SPI-based PMOD module
 *
 * Example: PmodOLEDrgb (display), PmodGYRO (gyroscope), PmodWiFi (WiFi)
 *
 * SPI PMOD Pinout (12-pin):
 * - Pin 1:   CS   (chip select)
 * - Pin 2:   COPI (controller out, peripheral in)
 * - Pin 3:   CIPO (controller in, peripheral out)
 * - Pin 4:   SCK  (SPI clock)
 * - Pin 7:   INT  (interrupt, optional)
 * - Pin 8:   RST  (reset, optional)
 * - Pin 5-6, 11-12: GND, VCC
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t init_spi_module(void)
{
  /* Example: Initialize SPI bus for display/sensor
   *
   * Note: Use rx_bus_manager with SPI interface
   * Example:
   *   rx_bus_manager_t bus_mgr = {0};
   *   rx_bus_config_t config = {
   *     .bus_type = k_rx_bus_spi,
   *     .spi.channel = k_rx_rspi_channel_0,
   *     .spi.clock_hz = 1000000,  // 1 MHz
   *     .spi.mode = k_rx_rspi_mode_0,
   *     .spi.bit_order = k_rx_rspi_msb_first,
   *   };
   *   rx_err_t ret = rx_bus_manager_init(&bus_mgr, &config);
   */

  rx_log_info(s_tag, "SPI module initialized");
  return k_rx_ok;
}

/* =============================================================================
 * Task Logic
 * =============================================================================
 */

/**
 * @brief Read sensor/input data
 *
 * Example task-specific logic for reading data from PMOD module.
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t read_module_data(void)
{
  /* Example: GPIO input (buttons/switches)
   *
   * bool button_pressed = gpio_read(k_port_b, 0);
   * if (button_pressed) {
   *   rx_log_info(s_tag, "Button 0 pressed");
   * }
   */

  /* Example: I2C sensor read (accelerometer)
   *
   * uint8_t data[6];
   * rx_err_t ret = rx_bus_manager_read(&bus_mgr, 0x32, data, 6);
   * if (ret == k_rx_ok) {
   *   int16_t x = (int16_t)((data[1] << 8) | data[0]);
   *   int16_t y = (int16_t)((data[3] << 8) | data[2]);
   *   int16_t z = (int16_t)((data[5] << 8) | data[4]);
   *   s_state.last_value = (float)x / 256.0f;  // Convert to g's
   * }
   */

  /* Example: SPI display write
   *
   * uint8_t cmd[] = {0x15, 0x00, 0x5F};  // Set column address
   * rx_err_t ret = rx_bus_manager_write(&bus_mgr, cmd, 3);
   */

  s_state.sample_count++;
  return k_rx_ok;
}

/**
 * @brief Write output data (LEDs, display, actuators)
 *
 * Example task-specific logic for writing to PMOD module.
 *
 * @return k_rx_ok on success, error code otherwise
 */
static rx_err_t write_module_data(void)
{
  /* Example: GPIO output (LEDs)
   *
   * static uint8_t led_state = 0;
   * led_state = (led_state + 1) % 16;  // 4 LEDs = 16 combinations
   * for (uint8_t i = 0; i < 4; i++) {
   *   if (led_state & (1 << i)) {
   *     gpio_write_high(k_port_b, i);
   *   } else {
   *     gpio_write_low(k_port_b, i);
   *   }
   * }
   */

  return k_rx_ok;
}

/**
 * @brief Update shared state (optional)
 *
 * If PMOD module needs to share data with other tasks, update shared_state here.
 *
 * Example: Store sensor data for telemetry transmission via Comm_Manager.
 */
static void update_shared_state(void)
{
  /* Example: Update health state with temperature sensor data
   *
   * shared_state_t* state = shared_state_get();
   * UINT status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
   * if (status == TX_SUCCESS) {
   *   state->health.temperature_celsius = s_state.last_value;
   *   tx_mutex_put(&state->health_mutex);
   * }
   */

  /* Example: Update safety state with obstacle detection
   *
   * shared_state_t* state = shared_state_get();
   * UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
   * if (status == TX_SUCCESS) {
   *   state->safety.obstacle_detected = s_state.fault;
   *   tx_mutex_put(&state->safety_mutex);
   * }
   */
}

/* =============================================================================
 * Thread Entry Point
 * =============================================================================
 */

/**
 * @brief PMOD template thread entry point
 *
 * Main loop for PMOD module:
 * 1. Register with watchdog (3x period timeout)
 * 2. Initialize hardware
 * 3. Main update loop:
 *    - Read sensor/input data
 *    - Process data
 *    - Write output data
 *    - Update shared state (if needed)
 *    - Send heartbeat to watchdog
 *    - Sleep until next update
 *
 * @param[in] input Thread input parameter (unused)
 */
static void pmod_template_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "PMOD template thread started");

  /* Calculate watchdog timeout (3x period for jitter tolerance)
   * Example: 10 Hz = 100ms period → 300ms timeout */
  const uint32_t period_ms = 1000 / k_update_rate_hz;
  const uint32_t watchdog_timeout_ms = period_ms * 3;

  /* Register with watchdog for task-level monitoring */
  rx_err_t ret = rx_iwdt_register_task("PMOD_Template", watchdog_timeout_ms);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Failed to register with watchdog");
  }

  /* Initialize hardware based on interface type
   * Note: Remove unused interface init functions */
  ret = init_gpio_module();  /* GPIO example */
  /* ret = init_i2c_module(); */  /* I2C example */
  /* ret = init_spi_module(); */  /* SPI example */

  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Hardware initialization failed");
    s_state.fault = true;

    /* Cannot continue - halt this thread */
    while (1) {
      tx_thread_sleep(k_threadx_ticks_1s);
    }
  }

  s_state.initialized = true;
  rx_log_info(s_tag, "Entering main update loop");

  /* Main update loop */
  while (1) {
    /* 1. Read sensor/input data */
    ret = read_module_data();
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Read failed");
      s_state.fault = true;
    }

    /* 2. Write output data (LEDs, display, etc.) */
    ret = write_module_data();
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Write failed");
      s_state.fault = true;
    }

    /* 3. Update shared state (optional) */
    update_shared_state();

    /* 4. Record task heartbeat for deadlock detection */
    rx_iwdt_task_heartbeat("PMOD_Template");

    /* 5. Sleep until next update
     * ThreadX tick = 10ms (100 Hz)
     * Example: 10 Hz → 100ms period → 10 ticks */
    const uint32_t sleep_ticks = period_ms / 10;
    tx_thread_sleep(sleep_ticks);
  }
}

/* =============================================================================
 * Public API
 * =============================================================================
 */

UINT pmod_template_create(void)
{
  /* Create PMOD template thread */
  UINT status = tx_thread_create(
      &s_pmod_template_thread,
      s_tag,
      pmod_template_entry,
      0,
      s_pmod_template_stack,
      sizeof(s_pmod_template_stack),
      k_priority_pmod_template,
      k_priority_pmod_template,
      TX_NO_TIME_SLICE,
      TX_AUTO_START
  );

  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Thread creation failed");
    return status;
  }

  rx_log_info(s_tag, "PMOD_Template thread created (priority 20, 10 Hz)");
  return TX_SUCCESS;
}

#endif /* Template disabled - do not compile */
