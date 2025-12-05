/* lib/star_bus/include/star_bus_onewire_constants.h */

#ifndef STAR_BUS_ONEWIRE_CONSTANTS_H
#define STAR_BUS_ONEWIRE_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OneWire protocol timing constants (in microseconds)
 *
 * These timing values are for standard speed OneWire communication.
 * All values are in microseconds and follow the Dallas/Maxim specifications.
 */
typedef struct {
  /* Reset and presence pulse timings */
  uint16_t reset_pulse_us;     /**< Master reset pulse duration (typ: 480) */
  uint16_t presence_wait_us;   /**< Wait time before sampling presence (typ: 70) */
  uint16_t presence_sample_us; /**< Total presence sampling window (typ: 410) */

  /* Write timing */
  uint16_t write_0_low_us;    /**< Write 0 low time (typ: 60) */
  uint16_t write_1_low_us;    /**< Write 1 low time (typ: 6) */
  uint16_t write_recovery_us; /**< Recovery time between write slots (typ: 10) */

  /* Read timing */
  uint16_t read_low_us;      /**< Master initiates read slot (typ: 6) */
  uint16_t read_sample_us;   /**< Sample time after read initiation (typ: 9) */
  uint16_t read_recovery_us; /**< Recovery time after read (typ: 55) */
} onewire_timing_t;

/* Standard speed OneWire timing constants */
static const onewire_timing_t s_onewire_timing_standard = {.reset_pulse_us     = 480,
                                                           .presence_wait_us   = 70,
                                                           .presence_sample_us = 410,
                                                           .write_0_low_us     = 60,
                                                           .write_1_low_us     = 6,
                                                           .write_recovery_us  = 10,
                                                           .read_low_us        = 6,
                                                           .read_sample_us     = 9,
                                                           .read_recovery_us   = 55};

/* Overdrive speed OneWire timing constants (for future use) */
static const onewire_timing_t s_onewire_timing_overdrive = {.reset_pulse_us     = 70,
                                                            .presence_wait_us   = 8,
                                                            .presence_sample_us = 40,
                                                            .write_0_low_us     = 8,
                                                            .write_1_low_us     = 1,
                                                            .write_recovery_us  = 1,
                                                            .read_low_us        = 1,
                                                            .read_sample_us     = 2,
                                                            .read_recovery_us   = 7};

/* Maximum number of OneWire buses supported */
static const uint8_t s_max_onewire_buses = 4;

/* Maximum number of OneWire peripheral buses supported */
static const uint8_t s_max_peripheral_buses = 2;

/* OneWire ROM commands */
typedef enum {
  k_onewire_cmd_search_rom   = 0xF0,
  k_onewire_cmd_read_rom     = 0x33,
  k_onewire_cmd_match_rom    = 0x55,
  k_onewire_cmd_skip_rom     = 0xCC,
  k_onewire_cmd_alarm_search = 0xEC
} onewire_rom_cmd_t;

/* DS18B20 specific commands (common OneWire device) */
typedef enum {
  k_ds18b20_cmd_convert_t     = 0x44,
  k_ds18b20_cmd_write_scratch = 0x4E,
  k_ds18b20_cmd_read_scratch  = 0xBE,
  k_ds18b20_cmd_copy_scratch  = 0x48,
  k_ds18b20_cmd_recall_e2     = 0xB8,
  k_ds18b20_cmd_read_power    = 0xB4
} ds18b20_cmd_t;

/* OneWire family codes for common devices */
typedef enum {
  k_onewire_family_ds18s20 = 0x10,
  k_onewire_family_ds18b20 = 0x28,
  k_onewire_family_ds1822  = 0x22,
  k_onewire_family_ds2430  = 0x14,
  k_onewire_family_ds2431  = 0x2D,
  k_onewire_family_ds2433  = 0x23,
  k_onewire_family_ds2438  = 0x26
} onewire_family_code_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_ONEWIRE_CONSTANTS_H */