/**
 * @file shared_state.c
 * @brief Shared state module implementation
 * @details
 * Provides mutex-protected shared state for inter-thread communication.
 * All state is statically allocated (no dynamic memory).
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "shared_state.h"
#include <string.h>

/* =============================================================================
 * Private State
 * =============================================================================
 */

/**
 * @brief Global shared state instance
 * @details Statically allocated, initialized in shared_state_init()
 */
static shared_state_t s_shared_state;

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

UINT shared_state_init(shared_state_t* state)
{
    UINT status = TX_SUCCESS;

    /* Validate parameter */
    if (state == NULL) {
        return TX_PTR_ERROR;
    }

    /* Zero-initialize all state structures */
    memset(state, 0, sizeof(shared_state_t));

    /* Initialize setpoint to invalid by default */
    state->setpoint.valid = false;

    /* Create setpoint mutex */
    status = tx_mutex_create(&state->setpoint_mutex, "SetpointMutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS) {
        return status;
    }

    /* Create encoder mutex */
    status = tx_mutex_create(&state->encoder_mutex, "EncoderMutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS) {
        tx_mutex_delete(&state->setpoint_mutex);
        return status;
    }

    /* Create safety mutex */
    status = tx_mutex_create(&state->safety_mutex, "SafetyMutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS) {
        tx_mutex_delete(&state->setpoint_mutex);
        tx_mutex_delete(&state->encoder_mutex);
        return status;
    }

    /* Create health mutex */
    status = tx_mutex_create(&state->health_mutex, "HealthMutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS) {
        tx_mutex_delete(&state->setpoint_mutex);
        tx_mutex_delete(&state->encoder_mutex);
        tx_mutex_delete(&state->safety_mutex);
        return status;
    }

    return TX_SUCCESS;
}

shared_state_t* shared_state_get(void)
{
    return &s_shared_state;
}
