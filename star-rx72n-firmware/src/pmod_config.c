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

static char s_tag[] = "pmod";

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
