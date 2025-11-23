/* lib/star_camera_imx219/include/star_camera_imx219.h */

#ifndef STAR_CAMERA_IMX219_H
#define STAR_CAMERA_IMX219_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_camera_imx219.h
 * @brief IMX219-83 stereo binocular camera sensor driver
 *
 * Features:
 * - Dual IMX219 8MP sensors (3280x2464)
 * - CSI-2 interface (MIPI)
 * - I2C control interface
 * - Stereo depth estimation support
 * - Configurable resolution and frame rate
 * - Auto white balance, auto exposure
 *
 * Resolutions supported:
 * - 3280x2464 @ 21 fps (full resolution)
 * - 1920x1080 @ 30 fps (1080p)
 * - 1640x1232 @ 40 fps
 * - 640x480 @ 90 fps (VGA)
 */

#define IMX219_I2C_ADDR_LEFT (0x10)
#define IMX219_I2C_ADDR_RIGHT (0x36)  // Typical second camera address
#define IMX219_CHIP_ID (0x0219)

typedef enum {
  IMX219_MODE_3280x2464_21FPS = 0,
  IMX219_MODE_1920x1080_30FPS,
  IMX219_MODE_1640x1232_40FPS,
  IMX219_MODE_640x480_90FPS,
} imx219_mode_t;

typedef enum {
  IMX219_FORMAT_RAW8 = 0,
  IMX219_FORMAT_RAW10,
  IMX219_FORMAT_RGB888,
} imx219_format_t;

typedef struct {
  uint16_t width;
  uint16_t height;
  uint8_t  fps;
  imx219_format_t format;
} imx219_resolution_config_t;

typedef struct {
  star_bus_manager_t* manager;
  const char*         bus_name;
  uint8_t             left_addr;
  uint8_t             right_addr;
  imx219_mode_t       mode;
  bool                enable_stereo;  // Enable both cameras for stereo
} imx219_config_t;

typedef struct {
  uint8_t* buffer;      // Image buffer
  size_t   buffer_size; // Buffer size in bytes
  uint16_t width;       // Image width
  uint16_t height;      // Image height
  uint64_t timestamp_us; // Capture timestamp
  imx219_format_t format;
} imx219_frame_t;

typedef struct imx219_handle {
  star_bus_manager_t* manager;
  const char*         bus_name;
  uint8_t             left_addr;
  uint8_t             right_addr;
  error_handler_t     error_handler;
  bool                initialized;
  bool                streaming;
  bool                stereo_enabled;
  imx219_mode_t       current_mode;
  imx219_resolution_config_t resolution;
  uint16_t            exposure;
  uint16_t            gain;
} imx219_handle_t;

/**
 * @brief Initialize IMX219 camera(s)
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_init(imx219_handle_t* handle, const imx219_config_t* config);

/**
 * @brief Deinitialize IMX219 camera(s)
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_deinit(imx219_handle_t* handle);

/**
 * @brief Set capture mode (resolution and frame rate)
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] mode   Capture mode
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_set_mode(imx219_handle_t* handle, imx219_mode_t mode);

/**
 * @brief Start video streaming
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_start_streaming(imx219_handle_t* handle);

/**
 * @brief Stop video streaming
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_stop_streaming(imx219_handle_t* handle);

/**
 * @brief Capture single frame from left camera
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] frame  Frame data structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_capture_left(const imx219_handle_t* handle, imx219_frame_t* frame);

/**
 * @brief Capture single frame from right camera
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] frame  Frame data structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_capture_right(const imx219_handle_t* handle, imx219_frame_t* frame);

/**
 * @brief Capture stereo frame pair (left + right)
 *
 * @param[in]  handle      Pointer to initialized handle
 * @param[out] frame_left  Left camera frame
 * @param[out] frame_right Right camera frame
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_capture_stereo(const imx219_handle_t* handle,
                                             imx219_frame_t*        frame_left,
                                             imx219_frame_t*        frame_right);

/**
 * @brief Set exposure time
 *
 * @param[in] handle      Pointer to initialized handle
 * @param[in] exposure_us Exposure time in microseconds
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_set_exposure(imx219_handle_t* handle, uint32_t exposure_us);

/**
 * @brief Set analog gain
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] gain   Analog gain (0-255)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_set_gain(imx219_handle_t* handle, uint16_t gain);

/**
 * @brief Enable auto exposure
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] enable true to enable auto exposure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_set_auto_exposure(imx219_handle_t* handle, bool enable);

/**
 * @brief Get current resolution configuration
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] config Resolution configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_camera_imx219_get_resolution(const imx219_handle_t*      handle,
                                             imx219_resolution_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* STAR_CAMERA_IMX219_H */
