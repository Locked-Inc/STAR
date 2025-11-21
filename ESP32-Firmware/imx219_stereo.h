/*
 * IMX219-83 Stereo Camera Driver for ESP32-IDF
 *
 * Comprehensive C header with security-hardened implementations
 * for dual 8MP IMX219 sensors with CSI-2 interface
 *
 * Author: Research-Based Driver Development
 * Date: 2025-11-20
 * License: MIT
 *
 * Security Considerations:
 * - All buffer sizes validated against overflow
 * - I2C register writes include read-back verification
 * - Resolution parameters whitelisted
 * - DMA buffers properly aligned and bounded
 * - Integer overflow checks on all calculations
 */

#ifndef __IMX219_STEREO_H__
#define __IMX219_STEREO_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION & CONSTANTS
 * ============================================================================
 */

#define IMX219_I2C_WRITE_ADDR       0x20  /* 8-bit write address */
#define IMX219_I2C_READ_ADDR        0x21  /* 8-bit read address */
#define IMX219_I2C_7BIT_ADDR        0x10  /* 7-bit address */

#define IMX219_CHIP_ID              0x0219
#define IMX219_CHIP_ID_HIGH         0x02
#define IMX219_CHIP_ID_LOW          0x19

/* Sensor Array Dimensions */
#define IMX219_NATIVE_WIDTH         3296
#define IMX219_NATIVE_HEIGHT        2480
#define IMX219_ACTIVE_WIDTH         3280
#define IMX219_ACTIVE_HEIGHT        2464

/* Maximum dimensions for buffer allocation */
#define MAX_IMAGE_WIDTH             3280
#define MAX_IMAGE_HEIGHT            2464
#define MAX_IMAGE_BYTES             (3280 * 2464 * 2)

/* DMA Buffer Configuration */
#define DMA_ALIGNMENT_BYTES         32
#define DMA_ALIGNMENT_MASK          (DMA_ALIGNMENT_BYTES - 1)
#define MAX_DMA_BUFFER_SIZE         (4 * 1024 * 1024)  /* 4MB limit */

/* CSI-2 Configuration */
#define CSI_LANES_2                 2
#define CSI_LANES_4                 4
#define CSI_DATA_TYPE_RAW8          0x2A
#define CSI_DATA_TYPE_RAW10         0x2B
#define CSI_DATA_TYPE_YUV422        0x1E

/* Timing Specifications */
#define IMX219_EXPOSURE_MIN         4      /* Minimum exposure in lines */
#define IMX219_EXPOSURE_MAX         65535  /* Maximum exposure in lines */
#define IMX219_ANALOG_GAIN_MIN      0      /* 1x gain */
#define IMX219_ANALOG_GAIN_MAX      232    /* ~11x gain */
#define IMX219_DIGITAL_GAIN_MIN     0x0100 /* 1x */
#define IMX219_DIGITAL_GAIN_MAX     0x0fff /* ~15.9x */

/* ============================================================================
 * I2C REGISTER MAP
 * ============================================================================
 */

typedef enum {
    /* Core Control */
    IMX219_REG_CHIP_ID              = 0x0000,
    IMX219_REG_MODE_SELECT          = 0x0100,  /* 0=standby, 1=streaming */
    IMX219_REG_RESET                = 0x0103,  /* Soft reset */

    /* Analog Gain */
    IMX219_REG_ANALOG_GAIN          = 0x0157,  /* 0-232 */

    /* Digital Gain */
    IMX219_REG_DIGITAL_GAIN_H       = 0x0158,
    IMX219_REG_DIGITAL_GAIN_L       = 0x0159,

    /* Exposure */
    IMX219_REG_EXPOSURE_H           = 0x015a,
    IMX219_REG_EXPOSURE_L           = 0x015b,

    /* Frame Timing */
    IMX219_REG_FRAME_LENGTH_H       = 0x0160,
    IMX219_REG_FRAME_LENGTH_L       = 0x0161,
    IMX219_REG_LINE_LENGTH_H        = 0x0162,
    IMX219_REG_LINE_LENGTH_L        = 0x0163,

    /* Video Format */
    IMX219_REG_X_ADD_START_H        = 0x0164,
    IMX219_REG_X_ADD_START_L        = 0x0165,
    IMX219_REG_Y_ADD_START_H        = 0x0166,
    IMX219_REG_Y_ADD_START_L        = 0x0167,
    IMX219_REG_X_OUTPUT_SIZE_H      = 0x0172,
    IMX219_REG_X_OUTPUT_SIZE_L      = 0x0173,
    IMX219_REG_Y_OUTPUT_SIZE_H      = 0x0174,
    IMX219_REG_Y_OUTPUT_SIZE_L      = 0x0175,

    /* CSI Configuration */
    IMX219_REG_CSI_LANE_MODE        = 0x0114,  /* 0=2-lane, 1=4-lane */

    /* Color Space */
    IMX219_REG_COLOR_SPACE          = 0x0101,

    /* Test Pattern */
    IMX219_REG_TEST_PATTERN         = 0x0600,
    IMX219_REG_TEST_PATTERN_DATA_H  = 0x0601,
    IMX219_REG_TEST_PATTERN_DATA_L  = 0x0602,
} imx219_reg_addr_t;

/* ============================================================================
 * ENUMERATIONS & TYPES
 * ============================================================================
 */

typedef enum {
    IMX219_RES_FULL = 0,    /* 3280x2464 @ 21.19 fps */
    IMX219_RES_1080P = 1,   /* 1920x1080 @ 47.57 fps */
    IMX219_RES_2MP = 2,     /* 1640x1232 @ 41.85 fps */
    IMX219_RES_VGA = 3,     /* 640x480 @ 206.65 fps */
    IMX219_RES_COUNT = 4,
} imx219_resolution_t;

typedef enum {
    IMX219_FMT_RAW10 = 0,
    IMX219_FMT_RAW8 = 1,
    IMX219_FMT_YUV422 = 2,
} imx219_pixel_format_t;

typedef enum {
    SYNC_MODE_NONE = 0,           /* Independent capture */
    SYNC_MODE_GPIO_FSYNC = 1,     /* Hardware frame sync */
    SYNC_MODE_I2C_BROADCAST = 2,  /* Software I2C sync */
    SYNC_MODE_CLOCK_SHARED = 3,   /* Shared clock */
} stereo_sync_mode_t;

/* ============================================================================
 * STRUCTURES
 * ============================================================================
 */

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t x_start;
    uint16_t y_start;
    uint16_t frame_length;
    uint32_t line_length;
    uint8_t binning_h;
    uint8_t binning_v;
    float max_fps;
    uint8_t csi_lanes;
} imx219_mode_t;

typedef struct {
    uint8_t *buffer;
    uint32_t allocated_bytes;
    uint16_t width;
    uint16_t height;
    uint16_t stride;
} image_buffer_t;

typedef struct {
    void *vaddr;
    uint32_t paddr;
    uint32_t size;
    uint32_t alignment;
} dma_buffer_info_t;

typedef struct {
    float focal_length_pixels;
    float baseline_mm;
    float principal_point_x;
    float principal_point_y;
    float min_disparity;
    float max_disparity;
} stereo_calibration_t;

typedef struct {
    uint16_t *disparity_map;
    uint16_t width;
    uint16_t height;
    uint32_t total_size_bytes;
} stereo_disparity_map_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    float disparity;
    float depth_mm;
} stereo_point_t;

typedef struct {
    float focal_length_pixels;
    float baseline_mm;
    float principal_point_x;
    float principal_point_y;
    float min_disparity;
    float max_disparity;
} stereo_calibration_params_t;

typedef struct {
    uint8_t ae_enabled;
    uint8_t target_brightness;
    uint8_t brightness_tolerance;
    uint16_t exp_step_size;
    float exp_step_factor;
    uint8_t max_analog_gain;
    uint16_t max_digital_gain;
    uint32_t convergence_frames;
    uint32_t frame_rate;
} ae_config_t;

typedef struct {
    stereo_sync_mode_t mode;
    gpio_num_t fsync_pin;
    uint32_t fsync_period_us;
    uint32_t sync_accuracy_us;
} stereo_sync_config_t;

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    gpio_num_t reset_gpio;
    gpio_num_t pwdn_gpio;
    const imx219_mode_t *current_mode;
    stereo_sync_config_t sync_config;
    stereo_calibration_t calib;
} imx219_driver_t;

/* ============================================================================
 * FUNCTION PROTOTYPES - I2C REGISTER ACCESS
 * ============================================================================
 */

/**
 * @brief Write to IMX219 register with I2C
 * @param port I2C port number
 * @param addr Register address (16-bit)
 * @param data Register value to write
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t imx219_write_reg(i2c_port_t port, uint16_t addr, uint8_t data);

/**
 * @brief Read from IMX219 register via I2C
 * @param port I2C port number
 * @param addr Register address (16-bit)
 * @param data Pointer to store read value
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t imx219_read_reg(i2c_port_t port, uint16_t addr, uint8_t *data);

/**
 * @brief Securely write register with validation and read-back
 * @param port I2C port number
 * @param addr Register address
 * @param value Register value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_FAIL on error
 */
esp_err_t imx219_secure_write_reg(i2c_port_t port, uint16_t addr, uint8_t value);

/**
 * @brief Validate register write parameters
 * @param addr Register address
 * @param value Register value to validate
 * @return ESP_OK if valid, ESP_ERR_INVALID_ARG otherwise
 */
esp_err_t validate_register_write(uint16_t addr, uint8_t value);

/* ============================================================================
 * FUNCTION PROTOTYPES - RESOLUTION & FORMAT
 * ============================================================================
 */

/**
 * @brief Validate resolution against supported modes
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param format Pixel format
 * @param out Output validated resolution structure
 * @return ESP_OK if supported, ESP_ERR_NOT_FOUND if unsupported
 */
esp_err_t validate_resolution(uint16_t width, uint16_t height,
                             uint8_t format, void *out);

/**
 * @brief Get mode structure for given resolution
 * @param resolution Resolution enum
 * @return Pointer to imx219_mode_t structure, NULL if invalid
 */
const imx219_mode_t *imx219_get_mode(imx219_resolution_t resolution);

/**
 * @brief Set camera resolution and capture mode
 * @param port I2C port
 * @param mode Mode structure with resolution settings
 * @return ESP_OK on success
 */
esp_err_t imx219_set_mode(i2c_port_t port, const imx219_mode_t *mode);

/* ============================================================================
 * FUNCTION PROTOTYPES - EXPOSURE & GAIN CONTROL
 * ============================================================================
 */

/**
 * @brief Set exposure time (in sensor scanlines)
 * @param port I2C port
 * @param exposure_lines Exposure time in lines (4-65535)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out of range
 */
esp_err_t imx219_set_exposure(i2c_port_t port, uint16_t exposure_lines);

/**
 * @brief Set analog gain (1x to ~11x)
 * @param port I2C port
 * @param gain_multiplier Desired gain multiplier (1.0-11.0)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out of range
 */
esp_err_t imx219_set_analog_gain(i2c_port_t port, float gain_multiplier);

/**
 * @brief Set digital gain
 * @param port I2C port
 * @param gain_db256 Digital gain in dB*256 units
 * @return ESP_OK on success
 */
esp_err_t imx219_set_digital_gain(i2c_port_t port, uint16_t gain_db256);

/**
 * @brief Convert register value to gain multiplier
 * @param reg_value Analog gain register value (0-232)
 * @return Gain multiplier (1.0x-11.0x)
 */
float imx219_analog_gain_to_multiplier(uint8_t reg_value);

/**
 * @brief Convert gain multiplier to register value
 * @param multiplier Desired gain multiplier
 * @return Register value (0-232)
 */
uint8_t imx219_gain_multiplier_to_register(float multiplier);

/**
 * @brief Initialize auto-exposure engine
 * @param ae Auto-exposure configuration structure
 * @param frame_rate Current frame rate in fps
 * @return ESP_OK on success
 */
esp_err_t ae_initialize(ae_config_t *ae, uint32_t frame_rate);

/**
 * @brief Compute exposure adjustment based on brightness
 * @param ae Auto-exposure configuration
 * @param current_brightness Current measured brightness (0-255)
 * @param exposure_adjust Output: adjusted exposure value
 * @return ESP_OK on success
 */
esp_err_t ae_compute_exposure(const ae_config_t *ae,
                              uint8_t current_brightness,
                              uint16_t *exposure_adjust);

/* ============================================================================
 * FUNCTION PROTOTYPES - BUFFER MANAGEMENT
 * ============================================================================
 */

/**
 * @brief Allocate image buffer with safety checks
 * @param buf Buffer structure to initialize
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param bytes_per_pixel Bytes per pixel (usually 1 or 2)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t image_buffer_allocate(image_buffer_t *buf,
                               uint16_t width, uint16_t height,
                               uint16_t bytes_per_pixel);

/**
 * @brief Validate buffer access bounds
 * @param buf Buffer structure
 * @param offset Offset in buffer
 * @param size Number of bytes to access
 * @return ESP_OK if access is valid
 */
esp_err_t image_buffer_validate_access(const image_buffer_t *buf,
                                      uint32_t offset, uint32_t size);

/**
 * @brief Free image buffer
 * @param buf Buffer structure to free
 */
void image_buffer_free(image_buffer_t *buf);

/**
 * @brief Allocate DMA buffer with proper alignment
 * @param buf DMA buffer info structure
 * @param size Requested size in bytes
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t allocate_dma_buffer(dma_buffer_info_t *buf, uint32_t size);

/**
 * @brief Validate DMA descriptor before use
 * @param buf DMA buffer info structure
 * @return ESP_OK if valid, ESP_FAIL otherwise
 */
esp_err_t validate_dma_descriptor(const dma_buffer_info_t *buf);

/**
 * @brief Free DMA buffer
 * @param buf DMA buffer info structure
 */
void free_dma_buffer(dma_buffer_info_t *buf);

/* ============================================================================
 * FUNCTION PROTOTYPES - STEREO SYNCHRONIZATION
 * ============================================================================
 */

/**
 * @brief Initialize hardware stereo synchronization
 * @param config Synchronization configuration
 * @return ESP_OK on success
 */
esp_err_t stereo_sync_init_hardware(stereo_sync_config_t *config);

/**
 * @brief Send frame sync trigger pulse
 * @param config Synchronization configuration
 * @return ESP_OK on success
 */
esp_err_t stereo_sync_trigger_frame(stereo_sync_config_t *config);

/**
 * @brief Synchronize cameras via I2C broadcast
 * @param port I2C port number
 * @return ESP_OK on success
 */
esp_err_t stereo_sync_i2c_broadcast(i2c_port_t port);

/**
 * @brief Verify clock synchronization between sensors
 * @param port I2C port number
 * @return ESP_OK if both sensors locked to clock
 */
esp_err_t verify_clock_synchronization(i2c_port_t port);

/**
 * @brief Validate stereo frame alignment
 * @param left_frame_id Left camera frame ID
 * @param right_frame_id Right camera frame ID
 * @param timestamp_diff_us Timestamp difference in microseconds
 * @return ESP_OK if frames are properly synchronized
 */
esp_err_t validate_stereo_frame_sync(uint32_t left_frame_id,
                                     uint32_t right_frame_id,
                                     int32_t timestamp_diff_us);

/* ============================================================================
 * FUNCTION PROTOTYPES - STEREO DEPTH CALCULATION
 * ============================================================================
 */

/**
 * @brief Calculate depth from disparity value
 * @param calib Stereo calibration parameters
 * @param disparity Disparity in pixels
 * @return Depth in mm (INFINITY if invalid)
 */
float calculate_depth_mm(const stereo_calibration_t *calib, float disparity);

/**
 * @brief Calculate required image width for depth range
 * @param min_depth_mm Minimum depth in mm
 * @param max_depth_mm Maximum depth in mm
 * @param calib Calibration parameters
 * @return Required width in pixels
 */
uint16_t calculate_required_image_width(float min_depth_mm,
                                        float max_depth_mm,
                                        const stereo_calibration_t *calib);

/**
 * @brief Allocate stereo disparity map
 * @param map Disparity map structure
 * @param width Map width in pixels
 * @param height Map height in pixels
 * @return ESP_OK on success
 */
esp_err_t allocate_disparity_map(stereo_disparity_map_t *map,
                                 uint16_t width, uint16_t height);

/**
 * @brief Compute disparity using block matching
 * @param left_image Left camera image data
 * @param right_image Right camera image data
 * @param width Image width
 * @param height Image height
 * @param block_size Matching block size in pixels
 * @param out_disparity Output disparity map
 * @return ESP_OK on success
 */
esp_err_t compute_disparity_block_match(
    const uint8_t *left_image,
    const uint8_t *right_image,
    uint16_t width, uint16_t height,
    uint16_t block_size,
    stereo_disparity_map_t *out_disparity);

/**
 * @brief Filter disparity map for valid depth range
 * @param map Disparity map
 * @param min_depth_mm Minimum valid depth
 * @param max_depth_mm Maximum valid depth
 * @param calib Calibration parameters
 * @return ESP_OK on success
 */
esp_err_t filter_disparity_map(stereo_disparity_map_t *map,
                               float min_depth_mm,
                               float max_depth_mm,
                               const stereo_calibration_t *calib);

/**
 * @brief Free disparity map
 * @param map Disparity map structure
 */
void free_disparity_map(stereo_disparity_map_t *map);

/* ============================================================================
 * FUNCTION PROTOTYPES - INITIALIZATION & CONTROL
 * ============================================================================
 */

/**
 * @brief Initialize IMX219 stereo camera system
 * @param driver Driver structure
 * @param i2c_port I2C port number
 * @param reset_gpio GPIO for reset line
 * @return ESP_OK on success
 */
esp_err_t imx219_stereo_init(imx219_driver_t *driver,
                            i2c_port_t i2c_port,
                            gpio_num_t reset_gpio);

/**
 * @brief Reset IMX219 sensor
 * @param driver Driver structure
 * @return ESP_OK on success
 */
esp_err_t imx219_sensor_reset(imx219_driver_t *driver);

/**
 * @brief Start streaming from sensor
 * @param driver Driver structure
 * @return ESP_OK on success
 */
esp_err_t imx219_start_streaming(imx219_driver_t *driver);

/**
 * @brief Stop streaming from sensor
 * @param driver Driver structure
 * @return ESP_OK on success
 */
esp_err_t imx219_stop_streaming(imx219_driver_t *driver);

/**
 * @brief Verify chip ID to confirm sensor presence
 * @param driver Driver structure
 * @return ESP_OK if chip ID matches, ESP_FAIL otherwise
 */
esp_err_t imx219_verify_chip_id(imx219_driver_t *driver);

/**
 * @brief Deinitialize and cleanup driver
 * @param driver Driver structure
 * @return ESP_OK on success
 */
esp_err_t imx219_stereo_deinit(imx219_driver_t *driver);

#ifdef __cplusplus
}
#endif

#endif /* __IMX219_STEREO_H__ */
