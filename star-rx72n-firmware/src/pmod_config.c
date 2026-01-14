/* src/pmod_config.c */

/**
 * @file pmod_config.c
 * @brief PMOD Configuration Implementation
 *
 * Implements PMOD task creation and configuration management.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "pmod_config.h"

#include "rx_log.h"

/* =============================================================================
 * Private Data
 * =============================================================================
 */

static const char* s_tag = "pmod";

/**
 * @brief PMOD connector configuration array
 *
 * Default configuration: All connectors set to k_pmod_none (unused).
 * To enable a module:
 * 1. Set type to desired module (e.g., k_pmod_oled_rgb)
 * 2. Set interface type (e.g., k_pmod_interface_spi)
 * 3. Set priority (10-31, see guidelines in pmod_config.h)
 * 4. Set stack size (typically 1024-4096 bytes)
 * 5. Set update rate (Hz, 0 = event-driven)
 * 6. Set task name (NULL = auto-generated "PMOD_JA", etc.)
 *
 * Example (enable OLED RGB display on JA):
 * @code
 * [k_pmod_connector_ja] = {
 *     .type = k_pmod_oled_rgb,
 *     .interface = k_pmod_interface_spi,
 *     .priority = 20,
 *     .stack_size = 2048,
 *     .update_rate_hz = 30,
 *     .task_name = "PMOD_Display",
 * },
 * @endcode
 */
const pmod_config_t g_pmod_connectors[k_pmod_connector_count] = {
  /* JA: Connector A (default: unused) */
  [k_pmod_connector_ja] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 20,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JB: Connector B (default: unused) */
  [k_pmod_connector_jb] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 21,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JC: Connector C (default: unused) */
  [k_pmod_connector_jc] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 22,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JD: Connector D (default: unused) */
  [k_pmod_connector_jd] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 23,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JE: Connector E (default: unused) */
  [k_pmod_connector_je] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 24,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JF: Connector F (default: unused) */
  [k_pmod_connector_jf] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 25,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },
};

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t pmod_create_all_tasks(void)
{
  uint8_t enabled_count = 0;

  /* Iterate through all connectors and create tasks for enabled modules */
  for (uint8_t i = 0; i < k_pmod_connector_count; i++) {
    const pmod_config_t* config = &g_pmod_connectors[i];

    /* Skip if module is not enabled */
    if (config->type == k_pmod_none) {
      continue;
    }

    enabled_count++;

    /* Create task for specific PMOD module type */
    switch (config->type) {
      /* Display modules (SPI) */
      case k_pmod_oled_rgb:
        rx_log_info(s_tag, "PMOD OLED RGB configured (not yet implemented)");
        break;
      case k_pmod_cls:
        rx_log_info(s_tag, "PMOD CLS configured (not yet implemented)");
        break;

      /* Sensor modules (I2C) */
      case k_pmod_acl:
        rx_log_info(s_tag, "PMOD ACL configured (not yet implemented)");
        break;
      case k_pmod_gyro:
        rx_log_info(s_tag, "PMOD GYRO configured (not yet implemented)");
        break;

      /* Communication modules */
      case k_pmod_wifi:
        rx_log_info(s_tag, "PMOD WiFi configured (not yet implemented)");
        break;
      case k_pmod_bt2:
        rx_log_info(s_tag, "PMOD BT2 configured (not yet implemented)");
        break;

      /* Input/Output modules (GPIO) */
      case k_pmod_led:
        rx_log_info(s_tag, "PMOD LED configured (not yet implemented)");
        break;
      case k_pmod_btn:
        rx_log_info(s_tag, "PMOD BTN configured (not yet implemented)");
        break;

      /* Motor/Actuator modules */
      case k_pmod_dhb1:
        rx_log_info(s_tag, "PMOD DHB1 configured (not yet implemented)");
        break;
      case k_pmod_step:
        rx_log_info(s_tag, "PMOD STEP configured (not yet implemented)");
        break;

      /* Audio modules */
      case k_pmod_i2s2:
        rx_log_info(s_tag, "PMOD I2S2 configured (not yet implemented)");
        break;
      case k_pmod_mic3:
        rx_log_info(s_tag, "PMOD MIC3 configured (not yet implemented)");
        break;

      /* Memory/Storage modules */
      case k_pmod_sd:
        rx_log_info(s_tag, "PMOD SD configured (not yet implemented)");
        break;
      case k_pmod_eeprom:
        rx_log_info(s_tag, "PMOD EEPROM configured (not yet implemented)");
        break;

      /* Custom modules */
      case k_pmod_custom_1:
      case k_pmod_custom_2:
      case k_pmod_custom_3:
      case k_pmod_custom_4:
        rx_log_info(s_tag, "PMOD custom module configured (not yet implemented)");
        break;

      /* Handle all other known types */
      case k_pmod_none:
        /* Should never reach here due to earlier check */
        break;

      default:
        rx_log_error(s_tag, "Unknown PMOD type");
        return k_rx_err_not_supported;
    }
  }

  if (enabled_count == 0) {
    rx_log_info(s_tag, "No PMOD modules enabled");
  }

  return k_rx_ok;
}

const pmod_config_t* pmod_get_config(pmod_connector_id_t connector)
{
  /* Validate connector ID */
  if (connector >= k_pmod_connector_count) {
    return NULL;
  }

  return &g_pmod_connectors[connector];
}

bool pmod_is_enabled(pmod_connector_id_t connector)
{
  /* Validate connector ID */
  if (connector >= k_pmod_connector_count) {
    return false;
  }

  /* Check if module type is not none */
  return (g_pmod_connectors[connector].type != k_pmod_none);
}
