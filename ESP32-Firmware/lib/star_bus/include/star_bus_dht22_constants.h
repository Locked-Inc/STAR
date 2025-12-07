/* lib/star_bus/include/star_bus_dht22_constants.h */

#ifndef STAR_BUS_DHT22_CONSTANTS_H
#define STAR_BUS_DHT22_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DHT22 protocol timing constants (in microseconds)
 *
 * These timing values are for the DHT22 temperature/humidity sensor
 * proprietary one-wire protocol (not Dallas OneWire).
 */
typedef struct {
  /* Start signal timings */
  uint32_t start_low_us;  /**< Start signal low duration (typ: 18000us = 18ms) */
  uint16_t start_high_us; /**< Start signal high duration (typ: 40us) */

  /* Response signal timings */
  uint16_t response_low_us;  /**< Sensor response low duration (typ: 80us) */
  uint16_t response_high_us; /**< Sensor response high duration (typ: 80us) */

  /* Data bit timings */
  uint16_t bit_start_us;  /**< Each bit starts with low pulse (typ: 50us) */
  uint16_t bit_0_high_us; /**< Bit 0 high duration (26-28us) */
  uint16_t bit_1_high_us; /**< Bit 1 high duration (typ: 70us) */

  /* Timeout values */
  uint32_t read_timeout_us; /**< Overall read timeout (typ: 100000us = 100ms) */
  uint16_t bit_timeout_us;  /**< Per-bit timeout (typ: 100us) */
} dht22_timing_t;

/* Standard DHT22 timing constants */
static const dht22_timing_t s_dht22_timing_standard = {
  .start_low_us     = 18000, /* 18ms start signal */
  .start_high_us    = 40,
  .response_low_us  = 80,
  .response_high_us = 80,
  .bit_start_us     = 50,
  .bit_0_high_us    = 28,
  .bit_1_high_us    = 70,
  .read_timeout_us  = 100000, /* 100ms total timeout */
  .bit_timeout_us   = 100};

/* DHT11 timing constants (compatible but different sensor) */
static const dht22_timing_t s_dht11_timing_standard = {
  .start_low_us     = 18000, /* 18ms start signal */
  .start_high_us    = 40,
  .response_low_us  = 80,
  .response_high_us = 80,
  .bit_start_us     = 50,
  .bit_0_high_us    = 28,
  .bit_1_high_us    = 70,
  .read_timeout_us  = 100000, /* 100ms total timeout */
  .bit_timeout_us   = 100};

/**
 * @brief DHT22 data structure
 */
typedef struct {
  uint8_t humidity_integral;    /**< Humidity integral part */
  uint8_t humidity_decimal;     /**< Humidity decimal part */
  uint8_t temperature_integral; /**< Temperature integral part */
  uint8_t temperature_decimal;  /**< Temperature decimal part */
  uint8_t checksum;             /**< Checksum byte */
} dht22_raw_data_t;

/**
 * @brief DHT22 sensor types
 */
typedef enum {
  k_dht_type_dht11  = 0,
  k_dht_type_dht22  = 1,
  k_dht_type_am2302 = 2 /* AM2302 is equivalent to DHT22 */
} dht_sensor_type_t;

/* Minimum interval between readings (milliseconds) */
static const uint32_t s_dht22_min_interval_ms = 2000; /* 2 seconds */
static const uint32_t s_dht11_min_interval_ms = 1000; /* 1 second */

/* Data frame size */
static const uint8_t s_dht22_data_bits  = 40; /* 5 bytes * 8 bits */
static const uint8_t s_dht22_data_bytes = 5;  /* Total bytes to read */

/* Maximum retries for communication */
static const uint8_t s_dht22_max_retries = 3;

/* Temperature and humidity ranges */
typedef struct {
  float temp_min_c;        /**< Minimum temperature in Celsius */
  float temp_max_c;        /**< Maximum temperature in Celsius */
  float humidity_min;      /**< Minimum relative humidity (%) */
  float humidity_max;      /**< Maximum relative humidity (%) */
  float temp_accuracy;     /**< Temperature accuracy (±°C) */
  float humidity_accuracy; /**< Humidity accuracy (±%) */
} dht_sensor_specs_t;

/* DHT22 specifications */
static const dht_sensor_specs_t s_dht22_specs = {.temp_min_c        = -40.0f,
                                                 .temp_max_c        = 80.0f,
                                                 .humidity_min      = 0.0f,
                                                 .humidity_max      = 100.0f,
                                                 .temp_accuracy     = 0.5f,
                                                 .humidity_accuracy = 2.0f};

/* DHT11 specifications */
static const dht_sensor_specs_t s_dht11_specs = {.temp_min_c        = 0.0f,
                                                 .temp_max_c        = 50.0f,
                                                 .humidity_min      = 20.0f,
                                                 .humidity_max      = 80.0f,
                                                 .temp_accuracy     = 2.0f,
                                                 .humidity_accuracy = 5.0f};

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_DHT22_CONSTANTS_H */