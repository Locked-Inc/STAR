// SPDX-License-Identifier: MIT
/**
 * @file boot_common.h
 * @brief Boot support header providing common definitions for SMC-generated boot files
 *
 * @details
 * Provides minimal definitions needed by boot files moved from smc_gen/.
 * Currently provides the INTERNAL_NOT_USED macro needed by SMC-generated boot files.
 *
 * @see platform.h Boot platform header that includes this file
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * @since Version 1.0.0
 */

#pragma once

/**
 * @def INTERNAL_NOT_USED
 * @brief Suppress "unused parameter" warnings for API-required parameters
 *
 * @details
 * Casts parameter to void to indicate it is intentionally unused. Used for
 * function parameters that must exist for API compatibility but are not used
 * in the implementation.
 *
 * @param[in] p Parameter to mark as unused
 *
 * @par Example:
 * @code
 * void callback(int event, void* context) {
 *     INTERNAL_NOT_USED(context);  // context unused in this handler
 *     handle_event(event);
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
#define INTERNAL_NOT_USED(p) ((void)(p))
