/* esp32-firmware/components/star_bus/star_bus_types.c */

#include "star_bus_types.h"

/* --- Public Functions --- */

const char* star_bus_type_to_string(star_bus_type_t type)
{
  /* Ensure the array size matches the enum count (excluding _count) */
  static const char* const star_bus_type_strings[k_star_bus_type_count] = {
    [k_star_bus_type_none] = "None",
    [k_star_bus_type_i2c]  = "I2C",
    [k_star_bus_type_spi]  = "SPI",
  };

  /* Use >= count for check as enum starts from 0 */
  if (type >= k_star_bus_type_count || type < k_star_bus_type_none) {
    return "Unknown";
  }

  return star_bus_type_strings[type];
}
