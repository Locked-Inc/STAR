/* lib/star_sensor_hcsr04/src/star_sensor_hcsr04_constants.c */

#include "star_sensor_hcsr04_constants.h"

/* Define the actual constants */
const uint32_t g_hcsr04_min_distance_cm        = 2U;
const uint32_t g_hcsr04_max_distance_cm        = 400U;
const float    g_hcsr04_speed_of_sound_cm_us   = 0.0343f;
const uint32_t g_hcsr04_timeout_us             = 23200U;
const uint32_t g_hcsr04_trigger_setup_us       = 2U;
const uint32_t g_hcsr04_trigger_pulse_us       = 10U;
const uint32_t g_hcsr04_poll_delay_short_us    = 10U;
const uint32_t g_hcsr04_poll_delay_med_us      = 50U;
const uint32_t g_hcsr04_measurement_timeout_ms = 60U;