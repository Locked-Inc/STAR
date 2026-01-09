/* include/tasks/pmod_template.h */

/**
 * @file pmod_template.h
 * @brief PMOD Task Template Header
 *
 * This is a template header file for PMOD expansion module tasks.
 * Copy this file and pmod_template.c to create new PMOD drivers.
 *
 * Steps to create a new PMOD module:
 * 1. Copy pmod_template.h/.c to pmod_<module>.h/.c
 * 2. Update MODULE_NAME and function names
 * 3. Configure in pmod_config.h (enable connector, set priority/stack)
 * 4. Implement hardware initialization and main loop
 * 5. Add source to CMakeLists.txt
 * 6. Call pmod_<module>_create() from tx_application_define()
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef PMOD_TEMPLATE_H
#define PMOD_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rx_err.h"
#include "tx_api.h"

/**
 * @brief Create PMOD template task
 *
 * Creates a ThreadX thread for the PMOD module with the configured
 * priority and stack size from pmod_config.h.
 *
 * Called from tx_application_define() after core system threads.
 *
 * @return TX_SUCCESS on success, error code otherwise
 */
UINT pmod_template_create(void);

#ifdef __cplusplus
}
#endif

#endif /* PMOD_TEMPLATE_H */
