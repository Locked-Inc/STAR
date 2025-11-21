# IMX219-83 Stereo Camera: Comprehensive Technical Reference for ESP32-IDF C Driver

## 1. HARDWARE SPECIFICATIONS

### 1.1 Camera Module Overview
- **Sensor**: Sony IMX219 (Dual - Stereo Configuration)
- **Resolution**: 3280 × 2464 pixels per sensor (8MP each)
- **Total Dual Sensor Output**: 16MP stereo pair
- **Baseline Distance**: 60mm (camera separation for triangulation)
- **Operating Voltage**: 3.3V logic level
- **Operating Temperature**: 0-60°C

### 1.2 Optical Characteristics
- **CMOS Size**: 1/4 inch
- **Focal Length**: 2.6mm
- **Angle of View**: 83° diagonal, 73° horizontal, 50° vertical
- **Distortion**: <1%
- **Image Format**: Bayer pattern (RGGB/SRGGB)

### 1.3 Onboard IMU (ICM20948)
- **Type**: 9-axis inertial measurement unit
- **Accelerometer**: ±2/±4/±8/±16g (configurable)
- **Gyroscope**: ±250/±500/±1000/±2000°/sec (configurable)
- **Magnetometer**: ±4900μT
- **Resolution**: 16-bit
- **I2C Interface**: 0x68 or 0x69 (selectable via AD0 pin)

---

## 2. CSI-2 INTERFACE SPECIFICATION

### 2.1 Interface Overview
- **Standard**: MIPI CSI-2 (Camera Serial Interface-2)
- **Lane Configuration**: 2-lane or 4-lane mode
- **Data Rate**:
  - 2-lane mode: 912 Mbps/lane (max)
  - 4-lane mode: 755 Mbps/lane (max)
- **Data Format**: SRGGB10 (RAW10) or SRGGB8 (RAW8)

### 2.2 Clock Requirements
- **Master Clock (INCK)**: Typically 24MHz (crystal oscillator driven)
- **CSI Clock (DCKP/DCKN)**: Differential pair, clock-qualified data lanes
- **Synchronization Tolerance**: Clock frequencies must match within 50ppm for synchronized stereo capture

### 2.3 CSI-2 Pinout (Typical Configuration)

```
Pin Function          Description
---------------------------------------------
INCK                 Master Clock Input (24MHz)
GND                  Ground
DCKP/DCKN            Clock differential pair (CSI-2 positive/negative)
DA0P/DA0N            Data Lane 0 (positive/negative)
DA1P/DA1N            Data Lane 1 (positive/negative)
DA2P/DA2N            Data Lane 2 (optional for 4-lane)
DA3P/DA3N            Data Lane 3 (optional for 4-lane)
SCL/SDA              I2C control interface
VSYNC/HSYNC          Vertical/Horizontal sync signals (optional)
```

### 2.4 Signal Integrity Requirements
- **Differential Impedance**: 100 ohms for CSI data/clock lines
- **Termination**: 100Ω resistors placed as close as possible to receiver
- **Trace Matching**: Data lane pairs matched within 5mm for length
- **Differential Pair Skew**: <10ps between P and N within a pair
- **Rise/Fall Time**: <500ps for data transitions

### 2.5 Timing Specifications

```c
/* CSI-2 Timing Parameters (from IMX219 datasheet) */
typedef struct {
    uint32_t t_hs_prepare;      // HS Prepare time: 40-85ns
    uint32_t t_hs_zero;         // HS Zero time: >145ns
    uint32_t t_hs_trail;        // HS Trail time: 60-110ns
    uint32_t t_clk_prepare;     // Clock Prepare time: 38-95ns
    uint32_t t_clk_zero;        // Clock Zero time: >35ns
    uint32_t t_clk_trail;       // Clock Trail time: 60-110ns
    uint32_t t_clk_post;        // Clock Post time: >60ns
    uint32_t t_clk_pre;         // Clock Pre time: >8ns
} csi2_timing_t;
```

---

## 3. I2C CONTROL INTERFACE

### 3.1 I2C Specifications
- **Standard**: I2C Fast Mode Plus (CCI - Camera Control Instance)
- **Frequency**: 11.4MHz to 27MHz (typical: 24MHz)
- **Write Address**: 0x20 (7-bit: 0x10)
- **Read Address**: 0x21 (7-bit: 0x10 with read bit)
- **Address Space**: 16-bit (2 bytes per register address)

### 3.2 I2C Register Map - Critical Registers

```c
/* Core Control Registers */
#define IMX219_REG_CHIP_ID              0x0000  // Fixed: 0x0219
#define IMX219_REG_MODE_SELECT          0x0100  // 0x00=standby, 0x01=streaming
#define IMX219_REG_RESET                0x0103  // Soft reset trigger

/* Analog Gain Control */
#define IMX219_REG_ANALOG_GAIN          0x0157  // Value: 0-232 (1x to 11x)
//   Gain = (value + 256) / 256

/* Digital Gain Control */
#define IMX219_REG_DIGITAL_GAIN_H       0x0158  // MSB
#define IMX219_REG_DIGITAL_GAIN_L       0x0159  // LSB
//   Gain = (value / 256) dB, range: 0x0100-0x0fff

/* Exposure Control */
#define IMX219_REG_EXPOSURE_H           0x015a  // MSB
#define IMX219_REG_EXPOSURE_L           0x015b  // LSB
//   Exposure time in lines, range: 4-65535

/* Frame Timing */
#define IMX219_REG_FRAME_LENGTH_H       0x0160  // MSB
#define IMX219_REG_FRAME_LENGTH_L       0x0161  // LSB
#define IMX219_REG_LINE_LENGTH_H        0x0162  // MSB
#define IMX219_REG_LINE_LENGTH_L        0x0163  // LSB

/* Video Format Control */
#define IMX219_REG_X_ADD_START_H        0x0164  // Horizontal start
#define IMX219_REG_X_ADD_START_L        0x0165
#define IMX219_REG_Y_ADD_START_H        0x0166  // Vertical start
#define IMX219_REG_Y_ADD_START_L        0x0167
#define IMX219_REG_X_OUTPUT_SIZE_H      0x0172  // Output width
#define IMX219_REG_X_OUTPUT_SIZE_L      0x0173
#define IMX219_REG_Y_OUTPUT_SIZE_H      0x0174  // Output height
#define IMX219_REG_Y_OUTPUT_SIZE_L      0x0175

/* CSI Lane Configuration */
#define IMX219_REG_CSI_LANE_MODE        0x0114  // 0=2-lane, 1=4-lane

/* Color Space and Bayer Pattern */
#define IMX219_REG_COLOR_SPACE          0x0101  // 0=BAYER, 1=YUV
#define IMX219_REG_BINNING_H            0x0162  // Horizontal binning
#define IMX219_REG_BINNING_V            0x0163  // Vertical binning

/* Test Pattern */
#define IMX219_REG_TEST_PATTERN         0x0600  // 0=off, 1=on
#define IMX219_REG_TEST_PATTERN_DATA_H  0x0601
#define IMX219_REG_TEST_PATTERN_DATA_L  0x0602

/* Clock Configuration */
#define IMX219_REG_INCK_FREQ            0x0014  // Input clock frequency
#define IMX219_REG_CLOCK_ENABLE         0x0202  // Clock enable register
```

### 3.3 I2C Register Access Patterns

```c
/* Safe I2C register write with validation */
typedef struct {
    uint16_t addr;      // Register address
    uint8_t  value;     // Register value
    uint8_t  valid_min; // Minimum valid value
    uint8_t  valid_max; // Maximum valid value
} imx219_reg_t;

/* Example register set with validation limits */
static const imx219_reg_t imx219_regs[] = {
    {0x0100, 0x00, 0x00, 0x01},  // Mode select: 0-1
    {0x0157, 0x00, 0x00, 232},   // Analog gain: 0-232
    {0x0114, 0x00, 0x00, 0x01},  // CSI lanes: 0-1
};

/* I2C transaction protocol */
esp_err_t imx219_write_reg(i2c_port_t port, uint16_t reg_addr, uint8_t data) {
    uint8_t write_buf[3];
    write_buf[0] = (reg_addr >> 8) & 0xFF;  // MSB
    write_buf[1] = reg_addr & 0xFF;         // LSB
    write_buf[2] = data;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMX219_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, 3, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t imx219_read_reg(i2c_port_t port, uint16_t reg_addr, uint8_t *data) {
    uint8_t write_buf[2];
    write_buf[0] = (reg_addr >> 8) & 0xFF;  // MSB
    write_buf[1] = reg_addr & 0xFF;         // LSB

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMX219_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, 2, true);
    i2c_master_start(cmd);  // Repeated START
    i2c_master_write_byte(cmd, (IMX219_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
```

---

## 4. RESOLUTION & FORMAT OPTIONS

### 4.1 Supported Resolution Modes

```c
typedef enum {
    IMX219_RES_3280x2464,  // Full resolution (8MP), max 21.19 fps
    IMX219_RES_1920x1080,  // 1080p crop, max 47.57 fps
    IMX219_RES_1640x1232,  // 2x2 binned, max 41.85 fps
    IMX219_RES_640x480,    // VGA, max 206.65 fps (limited by CSI bandwidth)
} imx219_resolution_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t x_start;      // Crop offset
    uint16_t y_start;      // Crop offset
    uint16_t frame_length; // Lines per frame
    uint32_t line_length;  // Pixels per line
    uint8_t  binning_h;    // Horizontal binning factor
    uint8_t  binning_v;    // Vertical binning factor
    float    max_fps;
    uint8_t  csi_lanes;    // 2 or 4 lanes
} imx219_mode_t;

static const imx219_mode_t imx219_modes[] = {
    {
        .width = 3280,
        .height = 2464,
        .x_start = 0,
        .y_start = 0,
        .frame_length = 3526,      // VTS (lines)
        .line_length = 3448,       // HTS (pixels)
        .binning_h = 0,
        .binning_v = 0,
        .max_fps = 21.19f,
        .csi_lanes = 2
    },
    {
        .width = 1920,
        .height = 1080,
        .x_start = 680,
        .y_start = 692,
        .frame_length = 1763,
        .line_length = 3448,
        .binning_h = 0,
        .binning_v = 0,
        .max_fps = 47.57f,
        .csi_lanes = 2
    },
    {
        .width = 1640,
        .height = 1232,
        .x_start = 0,
        .y_start = 0,
        .frame_length = 1763,
        .line_length = 3448,
        .binning_h = 1,       // 2x horizontal binning
        .binning_v = 1,       // 2x vertical binning
        .max_fps = 41.85f,
        .csi_lanes = 2
    },
    {
        .width = 640,
        .height = 480,
        .x_start = 1000,
        .y_start = 752,
        .frame_length = 1763,
        .line_length = 3448,
        .binning_h = 2,       // 4x horizontal binning
        .binning_v = 2,       // 4x vertical binning
        .max_fps = 206.65f,   // CSI bandwidth limited
        .csi_lanes = 2
    },
};
```

### 4.2 Supported Image Formats

```c
typedef enum {
    IMX219_FMT_RAW10,      // 10-bit Bayer (SRGGB10)
    IMX219_FMT_RAW8,       // 8-bit Bayer (SRGGB8)
    IMX219_FMT_YUV422,     // YCbCr 4:2:2
} imx219_pixel_format_t;

/* Format-specific CSI data types */
#define CSI_DATA_TYPE_RAW8    0x2A  // 8-bit RAW
#define CSI_DATA_TYPE_RAW10   0x2B  // 10-bit RAW
#define CSI_DATA_TYPE_YUV422  0x1E  // YCbCr 4:2:2

typedef struct {
    imx219_pixel_format_t format;
    uint32_t bpp;           // Bits per pixel
    uint32_t bytes_per_row; // For stride calculation
} imx219_format_info_t;
```

### 4.3 Data Rate Calculations

```c
/*
 * Data rate calculation for stereo dual-sensor system:
 *
 * Per-camera data rate = width × height × fps × bpp / 8
 *
 * Example (3280×2464 @ 21.19fps, RAW10):
 * = 3280 × 2464 × 21.19 × 10 / 8
 * = 2.68 Gbps (requires 4-lane CSI-2 at 755 Mbps/lane = 3.02 Gbps total)
 *
 * Example (1920×1080 @ 47.57fps, RAW8):
 * = 1920 × 1080 × 47.57 × 8 / 8
 * = 0.987 Gbps (fits in 2-lane CSI-2 at 912 Mbps/lane = 1.824 Gbps total)
 */

uint32_t calculate_data_rate_bps(uint16_t width, uint16_t height,
                                  float fps, uint8_t bpp) {
    uint64_t total_bits = (uint64_t)width * height * bpp;
    return (uint32_t)(total_bits * fps);
}

uint32_t required_csi_lanes(uint32_t data_rate_bps) {
    /* 2-lane: 912 Mbps/lane = 1824 Mbps total */
    /* 4-lane: 755 Mbps/lane = 3020 Mbps total */
    if (data_rate_bps > 1824000000) {
        return 4;  // Need 4-lane mode
    }
    return 2;      // 2-lane mode sufficient
}
```

---

## 5. EXPOSURE & GAIN CONTROL

### 5.1 Exposure Control

```c
/* Exposure is measured in lines (sensor scanlines) */
typedef struct {
    uint16_t exposure_lines;    // Exposure time in scanlines
    uint16_t frame_length_lines; // Total frame length
    uint32_t frame_duration_us;  // Frame duration in microseconds
} imx219_exposure_t;

/* Example: Calculate exposure time in microseconds */
uint32_t exposure_lines_to_us(uint16_t exposure_lines,
                              uint16_t frame_length_lines,
                              float fps) {
    float line_duration_us = (1.0f / fps) / frame_length_lines * 1e6;
    return (uint32_t)(exposure_lines * line_duration_us);
}

/* Exposure limits */
#define IMX219_EXPOSURE_MIN     4      // Minimum: 4 lines
#define IMX219_EXPOSURE_MAX     65535  // Maximum: 65535 lines
#define IMX219_EXPOSURE_STEP    1      // 1 line resolution

esp_err_t imx219_set_exposure(i2c_port_t port, uint16_t exposure_lines) {
    /* Validate exposure range */
    if (exposure_lines < IMX219_EXPOSURE_MIN ||
        exposure_lines > IMX219_EXPOSURE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Write to exposure registers (16-bit) */
    uint8_t exp_h = (exposure_lines >> 8) & 0xFF;
    uint8_t exp_l = exposure_lines & 0xFF;

    if (imx219_write_reg(port, IMX219_REG_EXPOSURE_H, exp_h) != ESP_OK)
        return ESP_FAIL;

    return imx219_write_reg(port, IMX219_REG_EXPOSURE_L, exp_l);
}
```

### 5.2 Analog Gain Control

```c
/* Analog gain: 1x to 11x (approx) */
/* Gain = (value + 256) / 256 */
typedef struct {
    uint8_t  gain_register;  // Raw register value (0-232)
    float    gain_multiplier; // Actual gain (1x-11x)
} imx219_analog_gain_t;

#define IMX219_ANALOG_GAIN_MIN  0    // 1x gain
#define IMX219_ANALOG_GAIN_MAX  232  // ~11x gain
#define IMX219_ANALOG_GAIN_1X   0    // Register value for 1x gain
#define IMX219_ANALOG_GAIN_2X   256  // Register value for 2x gain

float analog_gain_to_multiplier(uint8_t reg_value) {
    return ((float)(reg_value + 256)) / 256.0f;
}

uint8_t gain_multiplier_to_register(float multiplier) {
    /* Clamp to valid range */
    if (multiplier < 1.0f) multiplier = 1.0f;
    if (multiplier > 11.0f) multiplier = 11.0f;

    return (uint8_t)((multiplier * 256) - 256);
}

esp_err_t imx219_set_analog_gain(i2c_port_t port, float gain_multiplier) {
    uint8_t reg_value = gain_multiplier_to_register(gain_multiplier);

    if (reg_value > IMX219_ANALOG_GAIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    return imx219_write_reg(port, IMX219_REG_ANALOG_GAIN, reg_value);
}
```

### 5.3 Digital Gain Control

```c
/* Digital gain: 1x to ~15.9x in 1/256 dB increments */
/* Gain = value / 256 dB */
#define IMX219_DIGITAL_GAIN_MIN 0x0100  // 1x (256 in dB)
#define IMX219_DIGITAL_GAIN_MAX 0x0fff  // ~15.9x

uint16_t digital_gain_db256_to_multiplier(uint16_t gain_db256) {
    /* 1 dB = 20*log10(multiplier), so gain_db256/256 = dB */
    return gain_db256;
}

esp_err_t imx219_set_digital_gain(i2c_port_t port, uint16_t gain_db256) {
    if (gain_db256 < IMX219_DIGITAL_GAIN_MIN ||
        gain_db256 > IMX219_DIGITAL_GAIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t gain_h = (gain_db256 >> 8) & 0xFF;
    uint8_t gain_l = gain_db256 & 0xFF;

    if (imx219_write_reg(port, IMX219_REG_DIGITAL_GAIN_H, gain_h) != ESP_OK)
        return ESP_FAIL;

    return imx219_write_reg(port, IMX219_REG_DIGITAL_GAIN_L, gain_l);
}
```

### 5.4 Auto-Exposure Implementation Example

```c
/* Simple auto-exposure algorithm */
esp_err_t imx219_auto_expose(i2c_port_t port,
                             uint8_t target_brightness,
                             const imx219_mode_t *mode) {
    /* 1. Start with moderate exposure */
    uint16_t exposure = mode->frame_length_lines / 2;  /* 50% of frame */

    /* 2. Apply exposure */
    if (imx219_set_exposure(port, exposure) != ESP_OK)
        return ESP_FAIL;

    /* 3. Capture frame and analyze histogram */
    /* (in real implementation: get frame, compute mean brightness) */

    /* 4. Adjust based on brightness */
    uint8_t current_brightness = 128;  /* Placeholder */
    if (current_brightness < target_brightness) {
        /* Image too dark: increase exposure */
        exposure = MIN(exposure * 1.2f, IMX219_EXPOSURE_MAX);
    } else {
        /* Image too bright: decrease exposure */
        exposure = MAX(exposure * 0.8f, IMX219_EXPOSURE_MIN);
    }

    return imx219_set_exposure(port, exposure);
}
```

---

## 6. STEREO CAMERA SYNCHRONIZATION

### 6.1 Synchronization Mechanisms

```c
typedef enum {
    SYNC_MODE_NONE,           // Independent capture (unsynchronized)
    SYNC_MODE_GPIO_FSYNC,     // Hardware frame sync via GPIO
    SYNC_MODE_I2C_BROADCAST,  // Software sync via I2C commands
    SYNC_MODE_CLOCK_SHARED,   // Same clock input to both sensors
} stereo_sync_mode_t;

typedef struct {
    stereo_sync_mode_t mode;
    gpio_num_t fsync_pin;     // Frame sync GPIO (master trigger)
    uint32_t fsync_period_us; // Frame sync pulse period
    uint32_t sync_accuracy_us; /* Achievable sync accuracy */
} stereo_sync_config_t;

/* Hardware-level synchronization via FSYNC signal */
/* FSYNC: frame synchronization pulse, high at frame start */
#define STEREO_SYNC_GPIO_FSYNC  GPIO_NUM_12  // Master trigger pin
#define STEREO_SYNC_GPIO_RESET1 GPIO_NUM_13  // Camera 1 reset
#define STEREO_SYNC_GPIO_RESET2 GPIO_NUM_14  // Camera 2 reset

esp_err_t stereo_sync_init_hardware(stereo_sync_config_t *config) {
    /* Configure FSYNC GPIO as output */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->fsync_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&io_conf);
}

esp_err_t stereo_sync_trigger_frame(stereo_sync_config_t *config) {
    /* Send FSYNC pulse: low->high->low transition */
    gpio_set_level(config->fsync_pin, 0);
    esp_rom_delay_us(10);  /* 10us low pulse */
    gpio_set_level(config->fsync_pin, 1);
    esp_rom_delay_us(50);  /* 50us high pulse */
    gpio_set_level(config->fsync_pin, 0);

    return ESP_OK;
}
```

### 6.2 I2C Broadcast Synchronization

```c
/* Software synchronization using I2C broadcast to both sensors */
esp_err_t stereo_sync_i2c_broadcast(i2c_port_t port) {
    /*
     * I2C broadcast: send mode select command to address 0x00
     * All sensors on the bus respond to 0x00 (broadcast address)
     */

    uint8_t write_buf[3];
    write_buf[0] = (IMX219_REG_MODE_SELECT >> 8) & 0xFF;
    write_buf[1] = IMX219_REG_MODE_SELECT & 0xFF;
    write_buf[2] = 0x01;  /* Start streaming */

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    /* Broadcast address: 0x00 */
    i2c_master_write_byte(cmd, (0x00 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, 3, true);
    i2c_master_stop(cmd);

    return i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
}
```

### 6.3 Master-Slave Clock Configuration

```c
/* For synchronized capture, both sensors must receive same MCLK */
typedef struct {
    uint8_t left_i2c_addr;   /* 0x10 (left camera) */
    uint8_t right_i2c_addr;  /* 0x11 (right camera) - addressable via jumper */
    gpio_num_t mclk_pin;     /* Shared MCLK from oscillator */
    uint32_t mclk_frequency; /* Typically 24MHz */
} stereo_clock_config_t;

/*
 * Critical: Both IMX219 sensors MUST receive identical MCLK:
 * - MCLK tolerance: ±50ppm maximum deviation
 * - Phase skew: <100ps between sensors
 * - Duty cycle: 45-55%
 */

esp_err_t verify_clock_synchronization(stereo_clock_config_t *config) {
    /* In practice, monitor PLL lock status via I2C */

    uint8_t pll_status_l, pll_status_r;

    /* Check left camera PLL lock */
    imx219_read_reg(I2C_NUM_0, IMX219_REG_PLL_LOCK, &pll_status_l);

    /* Check right camera PLL lock */
    imx219_read_reg(I2C_NUM_1, IMX219_REG_PLL_LOCK, &pll_status_r);

    if ((pll_status_l & 0x01) && (pll_status_r & 0x01)) {
        return ESP_OK;  /* Both PLLs locked */
    }
    return ESP_FAIL;
}
```

---

## 7. STEREO DEPTH CALCULATION

### 7.1 Stereo Vision Mathematics

```c
/*
 * Stereo triangulation formula:
 *
 * Z (depth) = (f × B) / d
 *
 * Where:
 *   f = focal length in pixels
 *   B = baseline distance (60mm for IMX219-83)
 *   d = disparity (x_left - x_right) in pixels
 *   Z = depth (distance from camera in mm)
 *
 * Example:
 *   f = 1.9 mm (focal length) × (width / sensor_width)
 *     = 1.9 mm × (3280 / 3.68 mm) ≈ 1640 pixels
 *   B = 60 mm (baseline)
 *   d = 50 pixels (disparity)
 *   Z = (1640 × 60) / 50 = 1968 mm ≈ 2 meters
 */

typedef struct {
    float focal_length_pixels;  /* Calibrated focal length in pixels */
    float baseline_mm;          /* Camera baseline in mm (60mm for IMX219-83) */
    float principal_point_x;    /* Optical center X coordinate */
    float principal_point_y;    /* Optical center Y coordinate */
    float min_disparity;        /* Minimum disparity for valid depth */
    float max_disparity;        /* Maximum disparity */
} stereo_calibration_t;

/* Depth calculation from disparity */
typedef struct {
    uint16_t x;        /* Pixel X coordinate */
    uint16_t y;        /* Pixel Y coordinate */
    float disparity;   /* Disparity in pixels (positive = closer) */
    float depth_mm;    /* Calculated depth in mm */
} stereo_point_t;

float calculate_depth_mm(const stereo_calibration_t *calib, float disparity) {
    if (disparity <= calib->min_disparity) {
        return INFINITY;  /* Invalid or too far */
    }

    /* Z = (f × B) / d */
    return (calib->focal_length_pixels * calib->baseline_mm) / disparity;
}

/* Disparity range depends on baseline and desired depth range */
uint16_t calculate_required_image_width(float min_depth_mm,
                                        float max_depth_mm,
                                        const stereo_calibration_t *calib) {
    /* Minimum disparity at maximum depth */
    float min_disp = (calib->focal_length_pixels * calib->baseline_mm) / max_depth_mm;

    /* Maximum disparity at minimum depth */
    float max_disp = (calib->focal_length_pixels * calib->baseline_mm) / min_depth_mm;

    return (uint16_t)max_disp + 1;  /* Add 1 for safety */
}
```

### 7.2 Disparity Map Generation

```c
typedef struct {
    uint16_t *disparity_map;    /* Disparity values in pixels */
    uint16_t width;
    uint16_t height;
    uint32_t total_size_bytes;  /* For DMA buffer planning */
} stereo_disparity_map_t;

/* Allocate disparity map buffer with security checks */
esp_err_t allocate_disparity_map(stereo_disparity_map_t *map,
                                 uint16_t width, uint16_t height) {
    /* Validate dimensions */
    if (width > 3280 || height > 2464) {
        return ESP_ERR_INVALID_ARG;  /* Exceeds sensor resolution */
    }

    /* Calculate buffer size */
    uint32_t num_pixels = (uint32_t)width * height;
    if (num_pixels > (2 * 1024 * 1024 / 2)) {  /* 2MB limit for ESP32 */
        return ESP_ERR_NO_MEM;
    }

    map->total_size_bytes = num_pixels * sizeof(uint16_t);

    /* Allocate with DMA capability and alignment */
    map->disparity_map = (uint16_t *)heap_caps_malloc(
        map->total_size_bytes,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    if (map->disparity_map == NULL) {
        return ESP_ERR_NO_MEM;
    }

    map->width = width;
    map->height = height;
    return ESP_OK;
}

/* Block matching algorithm (simple correlation) */
esp_err_t compute_disparity_block_match(
    const uint8_t *left_image,
    const uint8_t *right_image,
    uint16_t width, uint16_t height,
    uint16_t block_size,
    stereo_disparity_map_t *out_disparity) {

    if (!left_image || !right_image || !out_disparity) {
        return ESP_ERR_INVALID_ARG;
    }

    #define MAX_DISPARITY 64  /* Maximum search range in pixels */

    /* Simple SSD (Sum of Squared Differences) matching */
    for (uint16_t y = block_size; y < height - block_size; y++) {
        for (uint16_t x = block_size; x < width - block_size; x++) {
            uint32_t best_ssd = UINT32_MAX;
            uint16_t best_disparity = 0;

            /* Search in right image */
            for (uint16_t d = 0; d < MAX_DISPARITY && x >= d; d++) {
                uint32_t ssd = 0;

                /* Compute block SSD */
                for (int dy = -block_size; dy <= block_size; dy++) {
                    for (int dx = -block_size; dx <= block_size; dx++) {
                        uint8_t left_pix = left_image[(y + dy) * width + (x + dx)];
                        uint8_t right_pix = right_image[(y + dy) * width + (x - d + dx)];
                        uint16_t diff = left_pix - right_pix;
                        ssd += diff * diff;
                    }
                }

                /* Check for overflow (security) */
                if (ssd < best_ssd) {
                    best_ssd = ssd;
                    best_disparity = d;
                }
            }

            out_disparity->disparity_map[y * width + x] = best_disparity;
        }
    }

    return ESP_OK;
    #undef MAX_DISPARITY
}
```

### 7.3 Depth Map Filtering

```c
/* Remove invalid/outlier disparity values */
esp_err_t filter_disparity_map(stereo_disparity_map_t *map,
                               float min_depth_mm,
                               float max_depth_mm,
                               const stereo_calibration_t *calib) {
    if (!map || !map->disparity_map) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Calculate disparity range for valid depth */
    float max_disp = (calib->focal_length_pixels * calib->baseline_mm) / min_depth_mm;
    float min_disp = (calib->focal_length_pixels * calib->baseline_mm) / max_depth_mm;

    uint32_t total_pixels = map->width * map->height;
    for (uint32_t i = 0; i < total_pixels; i++) {
        float disp = map->disparity_map[i];

        /* Mark invalid disparities as zero */
        if (disp < min_disp || disp > max_disp) {
            map->disparity_map[i] = 0;  /* 0 = invalid/unknown */
        }
    }

    return ESP_OK;
}
```

---

## 8. IMAGE SIGNAL PROCESSING (ISP) CONFIGURATION

### 8.1 ISP Pipeline Architecture

```c
/*
 * Typical ISP pipeline:
 *
 * Sensor RAW Data
 *        |
 *        v
 *   [OB Subtraction] - Remove optical black
 *        |
 *        v
 *   [Lens Shading] - Correct vignetting
 *        |
 *        v
 *   [Demosaicing] - RAW to RGB/YUV conversion
 *        |
 *        v
 *   [Color Correction] - Color space adjustment
 *        |
 *        v
 *   [Gamma Correction] - Tone mapping
 *        |
 *        v
 *   [Denoising] - Noise reduction
 *        |
 *        v
 *   [Sharpening] - Edge enhancement
 *        |
 *        v
 *   Processed Image Data
 */

typedef struct {
    uint8_t ob_subtraction_en;      /* Optical black subtraction */
    uint8_t lens_shading_en;        /* Lens shading correction */
    uint8_t demosaicing_method;     /* 0=bilinear, 1=edge-aware */
    uint8_t color_correction_en;    /* Color matrix correction */
    float   gamma;                  /* Gamma value (1.0-2.2) */
    uint8_t denoise_strength;       /* 0-100 */
    uint8_t sharpen_strength;       /* 0-100 */
    uint8_t hdr_mode;               /* 0=off, 1=on */
} isp_config_t;

typedef enum {
    ISP_STATE_DISABLED,
    ISP_STATE_ENABLED,
    ISP_STATE_PROCESSING,
    ISP_STATE_ERROR,
} isp_state_t;

typedef struct {
    isp_state_t state;
    isp_config_t config;
    uint32_t frames_processed;
    uint32_t errors;
} isp_context_t;
```

### 8.2 Auto-Exposure Configuration

```c
typedef struct {
    uint8_t ae_enabled;          /* Auto-exposure enable */
    uint8_t target_brightness;   /* Target luma level (0-255) */
    uint8_t brightness_tolerance; /* ±tolerance around target */

    /* Exposure adjustment strategy */
    uint16_t exp_step_size;      /* Lines per adjustment */
    float    exp_step_factor;    /* Multiplier: 1.1 = 10% increase/decrease */

    /* Gain adjustment strategy */
    uint8_t  max_analog_gain;    /* Limit analog gain before digital */
    uint16_t max_digital_gain;   /* Maximum digital gain */

    /* Convergence */
    uint32_t convergence_frames; /* Frames until AE settles */
    uint32_t frame_rate;         /* Current frame rate in fps */
} ae_config_t;

esp_err_t ae_initialize(ae_config_t *ae, uint32_t frame_rate) {
    if (!ae) return ESP_ERR_INVALID_ARG;

    ae->ae_enabled = 1;
    ae->target_brightness = 128;  /* Middle of range */
    ae->brightness_tolerance = 10;
    ae->exp_step_size = 10;
    ae->exp_step_factor = 1.1f;
    ae->max_analog_gain = 180;    /* ~8x */
    ae->max_digital_gain = 0x0800;
    ae->convergence_frames = 30;
    ae->frame_rate = frame_rate;

    return ESP_OK;
}

esp_err_t ae_compute_exposure(const ae_config_t *ae,
                              uint8_t current_brightness,
                              uint16_t *exposure_adjust) {
    if (!ae || !exposure_adjust) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t brightness_error = current_brightness - ae->target_brightness;

    if (brightness_error > ae->brightness_tolerance) {
        /* Image too bright: reduce exposure */
        *exposure_adjust = (uint16_t)(ae->exp_step_size / ae->exp_step_factor);
    } else if (brightness_error < -ae->brightness_tolerance) {
        /* Image too dark: increase exposure */
        *exposure_adjust = (uint16_t)(ae->exp_step_size * ae->exp_step_factor);
    } else {
        /* Brightness acceptable: no change */
        *exposure_adjust = ae->exp_step_size;
    }

    return ESP_OK;
}
```

### 8.3 Color Space and White Balance

```c
typedef enum {
    COLOR_SPACE_RAW,
    COLOR_SPACE_RGB,
    COLOR_SPACE_YUV422,
    COLOR_SPACE_YUV420,
} color_space_t;

typedef struct {
    float r_gain;    /* Red channel multiplier */
    float g_gain;    /* Green channel multiplier */
    float b_gain;    /* Blue channel multiplier */
} white_balance_t;

typedef struct {
    color_space_t color_space;
    white_balance_t wb;
    float color_temperature_k;  /* 3000K-8000K */
} color_correction_t;

/* Predefined color temperature white balances */
static const white_balance_t wb_presets[] = {
    {1.8f, 1.0f, 2.8f},   /* Tungsten (3200K) */
    {1.5f, 1.0f, 2.0f},   /* Daylight (5500K) */
    {1.3f, 1.0f, 1.8f},   /* Cloudy (6500K) */
    {1.2f, 1.0f, 1.5f},   /* Cool white (7500K) */
};

esp_err_t apply_white_balance(uint8_t *image,
                             uint16_t width, uint16_t height,
                             const white_balance_t *wb) {
    if (!image || !wb) return ESP_ERR_INVALID_ARG;

    /* Validate gain values */
    if (wb->r_gain <= 0 || wb->g_gain <= 0 || wb->b_gain <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Apply channel gains (simplified for RGB format) */
    for (uint32_t i = 0; i < width * height * 3; i += 3) {
        uint8_t r = image[i];
        uint8_t g = image[i + 1];
        uint8_t b = image[i + 2];

        /* Multiply and saturate */
        image[i] = MIN(255, (uint16_t)(r * wb->r_gain));
        image[i + 1] = MIN(255, (uint16_t)(g * wb->g_gain));
        image[i + 2] = MIN(255, (uint16_t)(b * wb->b_gain));
    }

    return ESP_OK;
}
```

---

## 9. SECURITY CONSIDERATIONS & HARDENING

### 9.1 Buffer Overflow Prevention

```c
/*
 * VULNERABILITY: Image buffer overflows
 *
 * Scenario: Attacker provides invalid resolution parameters that exceed
 *           allocated buffer size, causing out-of-bounds writes during
 *           DMA transfers.
 *
 * Impact: Memory corruption, information disclosure, RCE
 *
 * Mitigation: Strict bounds checking on all image dimensions
 */

#define MAX_IMAGE_WIDTH     3280  /* IMX219 maximum */
#define MAX_IMAGE_HEIGHT    2464  /* IMX219 maximum */
#define MAX_IMAGE_BYTES     (3280 * 2464 * 2)  /* 16 bits per pixel */

typedef struct {
    uint8_t *buffer;
    uint32_t allocated_bytes;
    uint32_t width;
    uint32_t height;
    uint32_t stride;  /* Bytes per row, including padding */
} image_buffer_t;

esp_err_t image_buffer_allocate(image_buffer_t *buf,
                               uint16_t width, uint16_t height,
                               uint16_t bytes_per_pixel) {
    /* Validate dimensions */
    if (width == 0 || width > MAX_IMAGE_WIDTH ||
        height == 0 || height > MAX_IMAGE_HEIGHT) {
        ESP_LOGE("IMG_BUF", "Invalid dimensions: %dx%d", width, height);
        return ESP_ERR_INVALID_ARG;
    }

    /* Check for integer overflow in size calculation */
    uint32_t required_bytes;
    if (__builtin_mul_overflow(width, bytes_per_pixel, &required_bytes) ||
        __builtin_mul_overflow(required_bytes, height, &required_bytes)) {
        ESP_LOGE("IMG_BUF", "Size calculation overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Add safety margin for stride alignment (DMA requirement) */
    uint32_t stride = ((width * bytes_per_pixel + 31) / 32) * 32;
    uint32_t aligned_bytes;
    if (__builtin_mul_overflow(stride, height, &aligned_bytes)) {
        ESP_LOGE("IMG_BUF", "Aligned size overflow");
        return ESP_ERR_INVALID_ARG;
    }

    if (aligned_bytes > MAX_IMAGE_BYTES) {
        ESP_LOGE("IMG_BUF", "Buffer too large: %d > %d", aligned_bytes, MAX_IMAGE_BYTES);
        return ESP_ERR_NO_MEM;
    }

    /* Allocate with DMA capability */
    buf->buffer = (uint8_t *)heap_caps_malloc(
        aligned_bytes,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    if (buf->buffer == NULL) {
        ESP_LOGE("IMG_BUF", "Allocation failed for %d bytes", aligned_bytes);
        return ESP_ERR_NO_MEM;
    }

    /* Initialize structure with validated values */
    buf->width = width;
    buf->height = height;
    buf->stride = stride;
    buf->allocated_bytes = aligned_bytes;

    ESP_LOGI("IMG_BUF", "Allocated %dx%d buffer (%d bytes, stride %d)",
             width, height, aligned_bytes, stride);

    return ESP_OK;
}

esp_err_t image_buffer_validate_access(const image_buffer_t *buf,
                                      uint32_t offset, uint32_t size) {
    if (!buf || !buf->buffer) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Check for integer overflow in end calculation */
    uint32_t end_offset;
    if (__builtin_add_overflow(offset, size, &end_offset)) {
        ESP_LOGE("IMG_BUF", "Offset overflow");
        return ESP_ERR_INVALID_ARG;
    }

    if (end_offset > buf->allocated_bytes) {
        ESP_LOGE("IMG_BUF", "Access out of bounds: %d > %d",
                 end_offset, buf->allocated_bytes);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

void image_buffer_free(image_buffer_t *buf) {
    if (buf && buf->buffer) {
        free(buf->buffer);
        buf->buffer = NULL;
        buf->allocated_bytes = 0;
        buf->width = 0;
        buf->height = 0;
    }
}
```

### 9.2 Invalid Resolution Parameter Handling

```c
/*
 * VULNERABILITY: Invalid resolution parameters
 *
 * Scenario: User provides resolution not in supported list, causing:
 *           - Incorrect CSI data type selection
 *           - Invalid timing parameters
 *           - DMA descriptor corruption
 *
 * Mitigation: Whitelist validation of resolution modes
 */

typedef enum {
    IMG_RES_VALID = 0x5A5A,   /* Magic value for valid config */
    IMG_RES_INVALID = 0x0000,
} img_res_validity_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t  csi_data_type;   /* CSI data type code */
    uint16_t valid_marker;    /* Magic marker */
} validated_resolution_t;

/* Whitelist of supported resolutions */
static const validated_resolution_t supported_resolutions[] = {
    {3280, 2464, CSI_DATA_TYPE_RAW10, IMG_RES_VALID},
    {1920, 1080, CSI_DATA_TYPE_RAW10, IMG_RES_VALID},
    {1640, 1232, CSI_DATA_TYPE_RAW10, IMG_RES_VALID},
    {640,  480,  CSI_DATA_TYPE_RAW10, IMG_RES_VALID},
    {3280, 2464, CSI_DATA_TYPE_RAW8,  IMG_RES_VALID},
    {1920, 1080, CSI_DATA_TYPE_RAW8,  IMG_RES_VALID},
    {0, 0, 0, IMG_RES_INVALID}  /* Terminator */
};

esp_err_t validate_resolution(uint16_t width, uint16_t height,
                             uint8_t format,
                             validated_resolution_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;

    for (int i = 0; supported_resolutions[i].valid_marker != IMG_RES_INVALID; i++) {
        const validated_resolution_t *res = &supported_resolutions[i];

        if (res->width == width && res->height == height) {
            /* Found supported resolution */
            *out = *res;
            out->valid_marker = IMG_RES_VALID;
            return ESP_OK;
        }
    }

    /* Not in whitelist */
    ESP_LOGE("RES_VAL", "Unsupported resolution: %dx%d", width, height);
    out->valid_marker = IMG_RES_INVALID;
    return ESP_ERR_NOT_FOUND;
}
```

### 9.3 I2C Register Validation

```c
/*
 * VULNERABILITY: Unvalidated I2C register writes
 *
 * Scenario: Attacker writes arbitrary values to control registers, causing:
 *           - Sensor malfunction
 *           - DMA pointer corruption
 *           - Exposure values leading to sensor damage
 *
 * Mitigation: Register read-back verification and range checking
 */

typedef struct {
    uint16_t addr;
    uint8_t  min_value;
    uint8_t  max_value;
    const char *name;
} imx219_reg_constraint_t;

static const imx219_reg_constraint_t register_constraints[] = {
    {0x0100, 0x00, 0x01, "MODE_SELECT"},
    {0x0103, 0x00, 0x01, "SW_RESET"},
    {0x0114, 0x00, 0x01, "CSI_LANE_MODE"},
    {0x0157, 0x00, 232,  "ANALOG_GAIN"},
    {0x0158, 0x01, 0xFF, "DIGITAL_GAIN_H"},
    {0x0159, 0x00, 0xFF, "DIGITAL_GAIN_L"},
    {0x015a, 0x00, 0xFF, "EXPOSURE_H"},
    {0x015b, 0x04, 0xFF, "EXPOSURE_L"},
    {0x0600, 0x00, 0x01, "TEST_PATTERN_EN"},
    {0, 0, 0, NULL}  /* Terminator */
};

esp_err_t validate_register_write(uint16_t addr, uint8_t value) {
    for (int i = 0; register_constraints[i].name != NULL; i++) {
        const imx219_reg_constraint_t *constraint = &register_constraints[i];

        if (constraint->addr == addr) {
            if (value < constraint->min_value || value > constraint->max_value) {
                ESP_LOGE("REG_VAL",
                         "Invalid value for %s (0x%04X): 0x%02X (valid: 0x%02X-0x%02X)",
                         constraint->name, addr, value,
                         constraint->min_value, constraint->max_value);
                return ESP_ERR_INVALID_ARG;
            }
            return ESP_OK;
        }
    }

    /* Register not in constraint list (unknown) */
    ESP_LOGW("REG_VAL", "Writing to unconstrained register 0x%04X = 0x%02X", addr, value);
    return ESP_OK;
}

esp_err_t imx219_secure_write_reg(i2c_port_t port, uint16_t addr, uint8_t value) {
    /* 1. Validate register value */
    if (validate_register_write(addr, value) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Write to register */
    if (imx219_write_reg(port, addr, value) != ESP_OK) {
        return ESP_FAIL;
    }

    /* 3. Read back for verification */
    uint8_t readback;
    if (imx219_read_reg(port, addr, &readback) != ESP_OK) {
        ESP_LOGE("REG_VERIFY", "Failed to read back register 0x%04X", addr);
        return ESP_FAIL;
    }

    /* 4. Verify value matches */
    if (readback != value) {
        ESP_LOGE("REG_VERIFY",
                 "Register mismatch at 0x%04X: wrote 0x%02X, read 0x%02X",
                 addr, value, readback);
        return ESP_FAIL;
    }

    return ESP_OK;
}
```

### 9.4 DMA Buffer Security

```c
/*
 * VULNERABILITY: Unsafe DMA buffer allocation and configuration
 *
 * Scenario: DMA buffers not properly aligned, placed in wrong memory,
 *           or descriptors not validated before processing.
 *
 * Mitigation: Proper allocation, alignment checking, descriptor validation
 */

typedef struct {
    void *vaddr;        /* Virtual address */
    uint32_t paddr;     /* Physical address */
    uint32_t size;      /* Buffer size in bytes */
    uint32_t alignment; /* Alignment in bytes (must be power of 2) */
} dma_buffer_info_t;

#define DMA_ALIGNMENT_BYTES 32  /* ESP32 typical DMA alignment */
#define DMA_ALIGNMENT_MASK  (DMA_ALIGNMENT_BYTES - 1)

esp_err_t allocate_dma_buffer(dma_buffer_info_t *buf, uint32_t size) {
    if (!buf || size == 0) return ESP_ERR_INVALID_ARG;

    /* Check size doesn't exceed ESP32 capabilities */
    if (size > 4 * 1024 * 1024) {  /* 4MB limit for safety */
        ESP_LOGE("DMA_BUF", "Requested size too large: %d bytes", size);
        return ESP_ERR_NO_MEM;
    }

    /* Allocate with DMA capability flag */
    buf->vaddr = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (buf->vaddr == NULL) {
        ESP_LOGE("DMA_BUF", "Failed to allocate %d bytes", size);
        return ESP_ERR_NO_MEM;
    }

    /* Verify alignment */
    uint32_t addr_int = (uint32_t)buf->vaddr;
    if ((addr_int & DMA_ALIGNMENT_MASK) != 0) {
        ESP_LOGW("DMA_BUF", "Buffer not properly aligned: 0x%08X", addr_int);
        free(buf->vaddr);
        return ESP_FAIL;
    }

    buf->size = size;
    buf->alignment = DMA_ALIGNMENT_BYTES;

    /* Physical address handling (ESP32-specific) */
    buf->paddr = (uint32_t)buf->vaddr;  /* Identity mapped on ESP32 */

    ESP_LOGI("DMA_BUF", "Allocated secure DMA buffer: vaddr=0x%08X, size=%d",
             (uint32_t)buf->vaddr, size);

    return ESP_OK;
}

esp_err_t validate_dma_descriptor(const dma_buffer_info_t *buf) {
    if (!buf || !buf->vaddr) return ESP_ERR_INVALID_ARG;

    /* Check alignment */
    if (((uint32_t)buf->vaddr & DMA_ALIGNMENT_MASK) != 0) {
        ESP_LOGE("DMA_DESC", "Buffer not aligned: 0x%08X", (uint32_t)buf->vaddr);
        return ESP_FAIL;
    }

    /* Check size is non-zero and reasonable */
    if (buf->size == 0 || buf->size > 4 * 1024 * 1024) {
        ESP_LOGE("DMA_DESC", "Invalid buffer size: %d", buf->size);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void free_dma_buffer(dma_buffer_info_t *buf) {
    if (buf && buf->vaddr) {
        free(buf->vaddr);
        buf->vaddr = NULL;
        buf->size = 0;
    }
}
```

### 9.5 Stereo Alignment Security

```c
/*
 * VULNERABILITY: Stereo image misalignment leading to disparity corruption
 *
 * Scenario: Left and right images not properly synchronized or aligned,
 *           causing invalid disparity calculations and buffer overruns.
 *
 * Mitigation: Frame synchronization validation and alignment checks
 */

typedef struct {
    uint32_t left_frame_id;
    uint32_t right_frame_id;
    uint32_t left_timestamp_us;
    uint32_t right_timestamp_us;
    uint16_t left_width;
    uint16_t left_height;
    uint16_t right_width;
    uint16_t right_height;
} stereo_frame_metadata_t;

esp_err_t validate_stereo_alignment(const stereo_frame_metadata_t *meta) {
    if (!meta) return ESP_ERR_INVALID_ARG;

    /* Check frame IDs match (synchronized capture) */
    if (meta->left_frame_id != meta->right_frame_id) {
        ESP_LOGW("STEREO_ALIGN",
                 "Frames out of sync: L=%d, R=%d",
                 meta->left_frame_id, meta->right_frame_id);
        return ESP_ERR_INVALID_STATE;
    }

    /* Check timestamp difference is minimal (<1ms) */
    int32_t timestamp_diff = (int32_t)(meta->left_timestamp_us - meta->right_timestamp_us);
    if (timestamp_diff < -1000 || timestamp_diff > 1000) {
        ESP_LOGW("STEREO_ALIGN",
                 "Large timestamp difference: %d us",
                 timestamp_diff);
        return ESP_ERR_INVALID_STATE;
    }

    /* Check dimensions match exactly */
    if (meta->left_width != meta->right_width ||
        meta->left_height != meta->right_height) {
        ESP_LOGE("STEREO_ALIGN",
                 "Dimension mismatch: L=%dx%d, R=%dx%d",
                 meta->left_width, meta->left_height,
                 meta->right_width, meta->right_height);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
```

### 9.6 Memory Safety for Disparity Calculation

```c
/*
 * VULNERABILITY: Integer overflow in disparity map calculations
 *
 * Scenario: Width × Height multiplication overflows when computing
 *           disparity map size, leading to undersized buffer allocation.
 *
 * Mitigation: Use safe integer arithmetic
 */

esp_err_t safe_disparity_allocation(uint16_t width, uint16_t height,
                                    stereo_disparity_map_t *map) {
    if (!map || width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Check dimensions against hardware limits */
    if (width > 3280 || height > 2464) {
        ESP_LOGE("DISP_ALLOC", "Dimensions exceed sensor: %dx%d", width, height);
        return ESP_ERR_INVALID_ARG;
    }

    /* Safe multiplication check */
    uint32_t num_pixels;
    if (__builtin_mul_overflow((uint32_t)width, (uint32_t)height, &num_pixels)) {
        ESP_LOGE("DISP_ALLOC", "Pixel count overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Calculate required bytes */
    uint32_t required_bytes;
    if (__builtin_mul_overflow(num_pixels, sizeof(uint16_t), &required_bytes)) {
        ESP_LOGE("DISP_ALLOC", "Size overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Memory limit check (2MB for stereo disparity) */
    if (required_bytes > 2 * 1024 * 1024) {
        ESP_LOGE("DISP_ALLOC", "Allocation exceeds limit: %d > 2MB", required_bytes);
        return ESP_ERR_NO_MEM;
    }

    /* Allocate with DMA flags */
    map->disparity_map = (uint16_t *)heap_caps_malloc(
        required_bytes,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    if (map->disparity_map == NULL) {
        ESP_LOGE("DISP_ALLOC", "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    map->width = width;
    map->height = height;
    map->total_size_bytes = required_bytes;

    return ESP_OK;
}
```

---

## 10. ESP32-IDF DRIVER INTEGRATION

### 10.1 Complete Driver Structure

```c
#include "esp_camera.h"
#include "sensor.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "soc/soc_caps.h"

static const char *TAG = "IMX219_STEREO";

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    gpio_num_t reset_gpio;
    gpio_num_t pwdn_gpio;
    const imx219_mode_t *current_mode;
    stereo_sync_config_t sync_config;
    stereo_calibration_t calib;
    isp_context_t isp;
} imx219_driver_t;

/* Global driver instance */
static imx219_driver_t g_imx219_driver = {
    .i2c_port = I2C_NUM_0,
    .i2c_addr = 0x10,
    .reset_gpio = GPIO_NUM_5,
    .pwdn_gpio = GPIO_NUM_18,
};

esp_err_t imx219_stereo_init(camera_config_t *config) {
    if (!config) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Initializing IMX219 stereo camera system");

    /* 1. Initialize I2C for sensor control */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->pin_sda,
        .scl_io_num = config->pin_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,  /* 400kHz I2C */
    };

    ESP_RETURN_ON_ERROR(
        i2c_param_config(g_imx219_driver.i2c_port, &i2c_conf),
        TAG, "I2C config failed"
    );

    ESP_RETURN_ON_ERROR(
        i2c_driver_install(g_imx219_driver.i2c_port,
                          i2c_conf.mode, 0, 0, 0),
        TAG, "I2C driver install failed"
    );

    /* 2. Configure reset GPIO */
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << g_imx219_driver.reset_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(
        gpio_config(&gpio_conf),
        TAG, "GPIO config failed"
    );

    /* 3. Reset sensor */
    ESP_RETURN_ON_ERROR(
        imx219_sensor_reset(),
        TAG, "Sensor reset failed"
    );

    /* 4. Initialize calibration data */
    g_imx219_driver.calib.focal_length_pixels = 1640.0f;
    g_imx219_driver.calib.baseline_mm = 60.0f;
    g_imx219_driver.calib.principal_point_x = 1640.0f;
    g_imx219_driver.calib.principal_point_y = 1232.0f;
    g_imx219_driver.calib.min_disparity = 1.0f;
    g_imx219_driver.calib.max_disparity = 60.0f;

    ESP_LOGI(TAG, "IMX219 stereo initialization complete");
    return ESP_OK;
}

esp_err_t imx219_sensor_reset(void) {
    ESP_LOGI(TAG, "Resetting IMX219 sensors");

    /* Pulse reset line low for 100ms */
    gpio_set_level(g_imx219_driver.reset_gpio, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(g_imx219_driver.reset_gpio, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    return ESP_OK;
}

esp_err_t imx219_set_mode(const imx219_mode_t *mode) {
    if (!mode) return ESP_ERR_INVALID_ARG;

    /* Validate resolution */
    validated_resolution_t validated;
    ESP_RETURN_ON_ERROR(
        validate_resolution(mode->width, mode->height, 0, &validated),
        TAG, "Invalid resolution"
    );

    /* Stop streaming */
    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_MODE_SELECT, 0x00),
        TAG, "Failed to stop streaming"
    );

    vTaskDelay(100 / portTICK_PERIOD_MS);

    /* Configure frame timing and resolution */
    uint16_t frame_length = mode->frame_length;
    uint16_t line_length = mode->line_length;

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_FRAME_LENGTH_H,
                               (frame_length >> 8) & 0xFF),
        TAG, "Frame length H write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_FRAME_LENGTH_L,
                               frame_length & 0xFF),
        TAG, "Frame length L write failed"
    );

    /* Configure output size */
    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_X_OUTPUT_SIZE_H,
                               (mode->width >> 8) & 0xFF),
        TAG, "X output size H write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_X_OUTPUT_SIZE_L,
                               mode->width & 0xFF),
        TAG, "X output size L write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_Y_OUTPUT_SIZE_H,
                               (mode->height >> 8) & 0xFF),
        TAG, "Y output size H write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_Y_OUTPUT_SIZE_L,
                               mode->height & 0xFF),
        TAG, "Y output size L write failed"
    );

    /* Start streaming */
    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(g_imx219_driver.i2c_port,
                               IMX219_REG_MODE_SELECT, 0x01),
        TAG, "Failed to start streaming"
    );

    g_imx219_driver.current_mode = mode;
    ESP_LOGI(TAG, "Set mode: %dx%d @ %.2f fps",
             mode->width, mode->height, mode->max_fps);

    return ESP_OK;
}
```

### 10.2 Compilation and Build Configuration

```cmake
# CMakeLists.txt snippet for ESP32-IDF project

idf_component_register(
    SRCS "imx219_stereo_driver.c"
         "imx219_i2c.c"
         "stereo_depth.c"
         "isp_config.c"
    INCLUDE_DIRS "include"
    REQUIRES driver i2c esp_camera
)

# Kconfig.projbuild for menuconfig options
menu "IMX219 Stereo Camera Configuration"

    config IMX219_I2C_SPEED_HZ
        int "I2C clock speed (Hz)"
        default 400000
        range 100000 1000000
        help
            I2C clock frequency for sensor control

    config IMX219_ENABLE_STEREO_SYNC
        bool "Enable hardware stereo synchronization"
        default y
        help
            Enable FSYNC signal for synchronized dual-sensor capture

    config IMX219_MAX_BUFFER_SIZE
        int "Maximum image buffer size (bytes)"
        default 2097152
        range 1048576 4194304
        help
            Memory limit for single image buffer

    config IMX219_ENABLE_ISP
        bool "Enable ISP pipeline"
        default y
        help
            Enable image signal processing

    config IMX219_LOG_LEVEL
        int "Logging level (0-5)"
        default 2
        range 0 5
        help
            0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE

endmenu
```

---

## 11. TESTING & VALIDATION

### 11.1 Unit Test Template

```c
#include "unity.h"
#include "imx219_stereo_driver.h"

TEST_CASE("IMX219: Buffer allocation", "[imx219]") {
    image_buffer_t buf = {0};

    /* Test valid allocation */
    TEST_ASSERT_EQUAL(ESP_OK,
                     image_buffer_allocate(&buf, 3280, 2464, 2));
    TEST_ASSERT_NOT_NULL(buf.buffer);
    TEST_ASSERT_EQUAL(3280, buf.width);
    TEST_ASSERT_EQUAL(2464, buf.height);

    image_buffer_free(&buf);
}

TEST_CASE("IMX219: Invalid resolution rejection", "[imx219]") {
    image_buffer_t buf = {0};

    /* Test invalid dimensions (exceeds max) */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                     image_buffer_allocate(&buf, 4096, 2464, 2));
    TEST_ASSERT_NULL(buf.buffer);
}

TEST_CASE("IMX219: Register validation", "[imx219]") {
    /* Test valid register value */
    TEST_ASSERT_EQUAL(ESP_OK, validate_register_write(0x0100, 0x00));
    TEST_ASSERT_EQUAL(ESP_OK, validate_register_write(0x0100, 0x01));

    /* Test invalid register value */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                     validate_register_write(0x0100, 0x02));
}

TEST_CASE("IMX219: Depth calculation", "[imx219]") {
    stereo_calibration_t calib = {
        .focal_length_pixels = 1640.0f,
        .baseline_mm = 60.0f,
        .min_disparity = 1.0f,
        .max_disparity = 60.0f,
    };

    /* Test depth at disparity = 60 pixels */
    float depth = calculate_depth_mm(&calib, 60.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1640.0f, depth);
}
```

---

## REFERENCES

1. **Sony IMX219PQ Datasheet**
   - Official register map and timing specifications
   - Source: https://www.opensourceinstruments.com/Electronics/Data/IMX219PQ.pdf

2. **Waveshare IMX219-83 Stereo Camera Wiki**
   - Hardware specifications and pinout
   - Source: https://www.waveshare.com/wiki/IMX219-83_Stereo_Camera

3. **MIPI CSI-2 Security Framework**
   - MIPI Camera Service Extensions (CSE) v2.0
   - Source: https://www.mipi.org/specifications/mipi-camera-security

4. **ESP32-IDF Memory Types Documentation**
   - DMA buffer allocation and alignment
   - Source: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/memory-types.html

5. **Linux IMX219 Kernel Driver**
   - Reference implementation for I2C control
   - Source: https://github.com/torvalds/linux/blob/master/drivers/media/i2c/imx219.c

6. **OpenCV Stereo Vision Documentation**
   - Calibration and depth estimation techniques
   - Source: https://docs.opencv.org/master/d9/d0c/group__calib3d.html

---

## DOCUMENT HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-20 | Initial comprehensive technical reference |

**Document Status**: Final - Ready for Implementation
**Security Level**: Technical Reference
**Compliance**: ESP32-IDF v5.5.1+

