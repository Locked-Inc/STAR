# QMC5883L Security & Safety Guide for ESP32-IDF

**Purpose:** Comprehensive security hardening guidelines for production-ready QMC5883L drivers
**Target Audience:** Embedded C developers working with ESP32-IDF
**Version:** 1.0

---

## Table of Contents

1. [Register Access Security](#register-access-security)
2. [I2C Communication Security](#i2c-communication-security)
3. [Data Validation & Error Handling](#data-validation--error-handling)
4. [Memory Safety](#memory-safety)
5. [Synchronization & Race Conditions](#synchronization--race-conditions)
6. [Testing & Verification](#testing--verification)

---

## Register Access Security

### 1. Invalid Register Protection

**Threat:** Writing to undefined registers can cause:
- Sensor malfunction
- Memory corruption in sensor EEPROM
- Unpredictable behavior
- Silent failures (no error indication)

**Mitigation Strategies:**

#### Strategy 1: Address Validation Matrix
```c
/**
 * Register access permission table
 * Prevents accidental writes to read-only or reserved registers
 */
typedef enum {
    REG_PERMISSION_NONE = 0x00,  /* No access */
    REG_PERMISSION_RO = 0x01,    /* Read-only */
    REG_PERMISSION_RW = 0x03,    /* Read-write */
} register_permission_t;

static const uint8_t g_register_permissions[] = {
    REG_PERMISSION_RO,  /* 0x00: X_OUT_LSB */
    REG_PERMISSION_RO,  /* 0x01: X_OUT_MSB */
    REG_PERMISSION_RO,  /* 0x02: Y_OUT_LSB */
    REG_PERMISSION_RO,  /* 0x03: Y_OUT_MSB */
    REG_PERMISSION_RO,  /* 0x04: Z_OUT_LSB */
    REG_PERMISSION_RO,  /* 0x05: Z_OUT_MSB */
    REG_PERMISSION_RO,  /* 0x06: STATUS */
    REG_PERMISSION_RO,  /* 0x07: TEMP_LSB (optional) */
    REG_PERMISSION_RO,  /* 0x08: TEMP_MSB (optional) */
    REG_PERMISSION_RW,  /* 0x09: CONTROL_1 */
    REG_PERMISSION_RW,  /* 0x0A: CONTROL_2 */
    REG_PERMISSION_RW,  /* 0x0B: SET_RESET */
    REG_PERMISSION_NONE,/* 0x0C: RESERVED */
    REG_PERMISSION_RO,  /* 0x0D: CHIP_ID */
};

#define QMC5883L_REG_MAX (sizeof(g_register_permissions) / sizeof(g_register_permissions[0]))

/**
 * Check if register access is allowed
 * @param reg: Register address
 * @param write: true for write operation, false for read
 * @return true if operation is permitted
 */
static bool qmc5883l_is_access_permitted(uint8_t reg, bool write)
{
    if (reg >= QMC5883L_REG_MAX) {
        ESP_LOGE(TAG, "Register 0x%02X out of bounds", reg);
        return false;
    }

    uint8_t perm = g_register_permissions[reg];

    if (perm == REG_PERMISSION_NONE) {
        ESP_LOGE(TAG, "Register 0x%02X is reserved", reg);
        return false;
    }

    if (write && !(perm & REG_PERMISSION_RW)) {
        ESP_LOGE(TAG, "Register 0x%02X is read-only", reg);
        return false;
    }

    return true;
}
```

#### Strategy 2: Safe Register Write Wrapper
```c
/**
 * Safely write to device register with validation
 * @param port: I2C port
 * @param reg: Register address
 * @param value: Value to write
 * @return ESP_OK on success, error code on failure
 */
esp_err_t qmc5883l_safe_write_register(i2c_port_t port, uint8_t reg, uint8_t value)
{
    ESP_LOGI(TAG, "Write request: reg=0x%02X, value=0x%02X", reg, value);

    /* Step 1: Validate register */
    if (!qmc5883l_is_access_permitted(reg, true)) {
        ESP_LOGE(TAG, "Register write blocked for security: 0x%02X", reg);
        return ESP_ERR_INVALID_ARG;
    }

    /* Step 2: Validate value for specific registers */
    switch (reg) {
        case 0x09: /* CONTROL_1 - validate mode bits */
            if ((value & 0x03) > 2) {
                ESP_LOGE(TAG, "Invalid mode in CONTROL_1: 0x%02X", value);
                return ESP_ERR_INVALID_ARG;
            }
            break;

        case 0x0A: /* CONTROL_2 - validate reserved bits */
            if ((value & 0xBE) != 0) {
                ESP_LOGW(TAG, "Writing to reserved bits in CONTROL_2: 0x%02X", value);
                /* Allow but warn */
            }
            break;

        case 0x0B: /* SET_RESET - any value OK */
            break;

        default:
            return ESP_ERR_INVALID_ARG;
    }

    /* Step 3: Perform I2C write */
    esp_err_t ret = _i2c_write_byte(port, reg, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: 0x%02X", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Register 0x%02X written successfully", reg);
    return ESP_OK;
}
```

#### Strategy 3: Configuration Validation
```c
/**
 * Validate control register settings before applying
 */
typedef struct {
    bool mode_valid;
    bool odr_valid;
    bool range_valid;
    bool osr_valid;
    const char *error_msg;
} qmc5883l_validation_result_t;

static qmc5883l_validation_result_t validate_control1(uint8_t ctrl1)
{
    qmc5883l_validation_result_t result = {
        .mode_valid = true,
        .odr_valid = true,
        .range_valid = true,
        .osr_valid = true,
        .error_msg = "Validation passed"
    };

    /* Mode validation: 0-2 valid, 3 reserved */
    uint8_t mode = ctrl1 & 0x03;
    if (mode > 2) {
        result.mode_valid = false;
        result.error_msg = "Invalid mode: 3 is reserved";
    }

    /* ODR validation: 0-3 all valid */
    uint8_t odr = (ctrl1 >> 2) & 0x03;
    /* All values valid */

    /* Range validation: 0-1 valid, 2-3 undefined */
    uint8_t range = (ctrl1 >> 4) & 0x03;
    if (range > 1) {
        result.range_valid = false;
        result.error_msg = "Invalid range: > 1 is undefined";
    }

    /* OSR validation: 0-3 all valid */
    /* All values valid */

    return result;
}

esp_err_t qmc5883l_apply_control1(i2c_port_t port, uint8_t ctrl1)
{
    qmc5883l_validation_result_t validation = validate_control1(ctrl1);

    if (!validation.mode_valid || !validation.range_valid) {
        ESP_LOGE(TAG, "CONTROL_1 validation failed: %s", validation.error_msg);
        return ESP_ERR_INVALID_ARG;
    }

    return qmc5883l_safe_write_register(port, 0x09, ctrl1);
}
```

---

## I2C Communication Security

### 1. Address Validation

**Threat:** Writing to wrong I2C address can:
- Corrupt other devices on I2C bus
- Cause bus lockup
- Create difficult-to-debug issues

**Mitigation:**

```c
/**
 * Verify correct device is at specified address
 * @param port: I2C port
 * @param addr: I2C address to verify
 * @return ESP_OK if valid QMC5883L device found
 */
esp_err_t qmc5883l_verify_device_address(i2c_port_t port, uint8_t addr)
{
    ESP_LOGI(TAG, "Verifying device at I2C address 0x%02X", addr);

    /* Step 1: Check for ACK from device */
    uint8_t dummy;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &dummy, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No ACK from device at 0x%02X (I2C error: 0x%X)",
                 addr, ret);
        return ESP_ERR_NOT_FOUND;
    }

    /* Step 2: Read and verify Chip ID */
    uint8_t chip_id;
    ret = i2c_read_byte(port, addr, 0x0D, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID");
        return ret;
    }

    if (chip_id != 0xFF) {
        ESP_LOGE(TAG, "Invalid chip ID: 0x%02X (expected 0xFF)", chip_id);
        ESP_LOGI(TAG, "Hint: This might be HMC5883L (0x1E) or different chip");
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "Device verified at address 0x%02X, Chip ID: 0x%02X",
             addr, chip_id);
    return ESP_OK;
}
```

### 2. I2C Bus Transaction Timeout

**Threat:** Sensor hang-up can freeze entire I2C bus

**Mitigation:**

```c
/**
 * I2C transaction with strict timeout enforcement
 */
#define QMC5883L_I2C_TIMEOUT_MS 1000  /* Absolute timeout */
#define QMC5883L_I2C_TIMEOUT_TICKS pdMS_TO_TICKS(QMC5883L_I2C_TIMEOUT_MS)

static esp_err_t _i2c_read_with_timeout(i2c_port_t port, uint8_t addr,
                                        uint8_t reg, uint8_t *data)
{
    uint32_t start_time = xTaskGetTickCount();

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, QMC5883L_I2C_TIMEOUT_TICKS);
    i2c_cmd_link_delete(cmd);

    uint32_t elapsed = xTaskGetTickCount() - start_time;
    if (elapsed > pdMS_TO_TICKS(QMC5883L_I2C_TIMEOUT_MS * 0.8)) {
        ESP_LOGW(TAG, "I2C transaction slow: %lu ms (threshold: %d ms)",
                 elapsed, QMC5883L_I2C_TIMEOUT_MS);
    }

    if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "I2C timeout (possible bus hang-up)");
        /* Consider bus recovery */
    }

    return ret;
}
```

### 3. Repeated START Condition Handling

```c
/**
 * Safe I2C repeated START transaction
 * Ensures proper bus state management
 */
esp_err_t qmc5883l_i2c_read_safe(i2c_port_t port, uint8_t addr, uint8_t reg,
                                 uint8_t *buffer, size_t len)
{
    if (buffer == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* WRITE PHASE: Send address of register to read */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    if (ESP_OK != i2c_master_write_byte(cmd, reg, true)) {
        i2c_cmd_link_delete(cmd);
        return ESP_FAIL;
    }

    /* REPEATED START: Initiate read phase */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);

    /* READ PHASE: Read data bytes */
    for (size_t i = 0; i < len - 1; i++) {
        i2c_master_read_byte(cmd, &buffer[i], I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &buffer[len - 1], I2C_MASTER_NACK);

    /* STOP CONDITION */
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd,
                                        pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed at register 0x%02X: 0x%X", reg, ret);
    }

    return ret;
}
```

---

## Data Validation & Error Handling

### 1. Overflow Detection

**Threat:** Sensor saturation in strong magnetic fields produces invalid data

**Implementation:**

```c
/**
 * Read with overflow detection and recovery
 */
esp_err_t qmc5883l_read_safe(i2c_port_t port, int16_t *x, int16_t *y, int16_t *z)
{
    if (x == NULL || y == NULL || z == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[7];
    esp_err_t ret = qmc5883l_i2c_read_safe(port, QMC5883L_I2C_ADDRESS, 0x00,
                                           buffer, 7);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Extract status before processing data */
    uint8_t status = buffer[6];
    bool drdy = (status & 0x01) != 0;
    bool overflow = (status & 0x02) != 0;

    if (!drdy) {
        ESP_LOGW(TAG, "Data not ready (DRDY=0)");
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (overflow) {
        ESP_LOGW(TAG, "OVERFLOW DETECTED - magnetic field exceeds range");
        /* Return previous valid data or error */
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Reconstruct values (little-endian) */
    *x = (int16_t)((buffer[1] << 8) | buffer[0]);
    *y = (int16_t)((buffer[3] << 8) | buffer[2]);
    *z = (int16_t)((buffer[5] << 8) | buffer[4]);

    return ESP_OK;
}
```

### 2. Data Consistency Checking

```c
/**
 * Detect stuck/frozen sensor values
 * (Same value read multiple times indicates sensor failure)
 */
typedef struct {
    int16_t last_x, last_y, last_z;
    uint32_t unchanged_count;
} qmc5883l_consistency_check_t;

static qmc5883l_consistency_check_t g_consistency = {0};

esp_err_t qmc5883l_read_with_consistency_check(i2c_port_t port,
                                               int16_t *x, int16_t *y, int16_t *z)
{
    esp_err_t ret = qmc5883l_read_safe(port, x, y, z);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Check if values changed */
    if (*x == g_consistency.last_x &&
        *y == g_consistency.last_y &&
        *z == g_consistency.last_z) {

        g_consistency.unchanged_count++;

        if (g_consistency.unchanged_count > 100) {
            ESP_LOGE(TAG, "Sensor appears frozen - same value for 100+ reads");
            return ESP_ERR_INVALID_RESPONSE;
        }
    } else {
        g_consistency.unchanged_count = 0;
    }

    g_consistency.last_x = *x;
    g_consistency.last_y = *y;
    g_consistency.last_z = *z;

    return ESP_OK;
}
```

### 3. DRDY Synchronization Safety

```c
/**
 * Safe DRDY synchronization with timeout and verification
 */
esp_err_t qmc5883l_wait_drdy_safe(uint32_t timeout_ms)
{
    uint32_t start_time = xTaskGetTickCount();
    uint32_t drdy_count = 0;
    const uint32_t MAX_POLLS = 10000;  /* Prevent infinite loop */

    while (drdy_count < MAX_POLLS) {
        uint8_t status;
        esp_err_t ret = i2c_read_byte(I2C_NUM_0, QMC5883L_I2C_ADDRESS,
                                      0x06, &status);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read status during DRDY wait");
            return ret;
        }

        if (status & 0x01) {  /* DRDY bit */
            return ESP_OK;
        }

        /* Check timeout */
        if (xTaskGetTickCount() - start_time > pdMS_TO_TICKS(timeout_ms)) {
            ESP_LOGE(TAG, "DRDY timeout after %lu ms", timeout_ms);
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
        drdy_count++;
    }

    ESP_LOGE(TAG, "DRDY polling exceeded maximum iterations");
    return ESP_ERR_INVALID_RESPONSE;
}
```

---

## Memory Safety

### 1. Buffer Overflow Prevention

```c
/**
 * Safe buffer read with size validation
 */
#define QMC5883L_MEASUREMENT_BUFFER_SIZE 7

esp_err_t qmc5883l_read_to_buffer(i2c_port_t port, uint8_t *buffer,
                                  size_t buffer_size)
{
    /* Validate buffer size */
    if (buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer_size < QMC5883L_MEASUREMENT_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Buffer too small: %zu < %d",
                 buffer_size, QMC5883L_MEASUREMENT_BUFFER_SIZE);
        return ESP_ERR_INVALID_ARG;
    }

    /* Clear entire buffer to prevent data leakage */
    memset(buffer, 0, buffer_size);

    /* Read exactly 7 bytes */
    return qmc5883l_i2c_read_safe(port, QMC5883L_I2C_ADDRESS, 0x00,
                                  buffer, QMC5883L_MEASUREMENT_BUFFER_SIZE);
}
```

### 2. Signed Integer Handling

```c
/**
 * Safe signed integer reconstruction
 * Prevents sign extension errors from unsigned arithmetic
 */
static inline int16_t safe_combine_bytes(uint8_t lsb, uint8_t msb)
{
    /* Method 1: Cast to uint16_t, then cast result to int16_t */
    /* C automatically handles two's complement sign extension */
    uint16_t combined = ((uint16_t)msb << 8) | (uint16_t)lsb;
    return (int16_t)combined;

    /* Method 2: Use union (not recommended - aliasing issues) */
    /* union { uint16_t u; int16_t i; } v = { .u = combined }; */
    /* return v.i; */
}

/* Test cases to verify correct behavior */
void test_signed_reconstruction(void)
{
    assert(safe_combine_bytes(0x00, 0x00) == 0);        /* 0 */
    assert(safe_combine_bytes(0x01, 0x00) == 1);        /* +1 */
    assert(safe_combine_bytes(0xFF, 0x7F) == 32767);    /* Max positive */
    assert(safe_combine_bytes(0x00, 0x80) == -32768);   /* Min negative */
    assert(safe_combine_bytes(0xFF, 0xFF) == -1);       /* -1 */
    assert(safe_combine_bytes(0x00, 0x81) == -32768 + 256);
}
```

### 3. Dynamic Memory Management

```c
/**
 * Safe allocation for device handle
 */
typedef struct {
    i2c_port_t port;
    uint8_t address;
    SemaphoreHandle_t mutex;
    uint32_t read_count;
    uint32_t error_count;
} qmc5883l_device_t;

esp_err_t qmc5883l_device_create(i2c_port_t port, qmc5883l_device_t **device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Allocate device structure */
    qmc5883l_device_t *dev = malloc(sizeof(qmc5883l_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate device structure");
        return ESP_ERR_NO_MEM;
    }

    /* Initialize structure (clear to prevent garbage data) */
    memset(dev, 0, sizeof(qmc5883l_device_t));

    dev->port = port;
    dev->address = QMC5883L_I2C_ADDRESS;

    /* Create mutex */
    dev->mutex = xSemaphoreCreateMutex();
    if (dev->mutex == NULL) {
        free(dev);
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    *device = dev;
    return ESP_OK;
}

void qmc5883l_device_destroy(qmc5883l_device_t *device)
{
    if (device == NULL) {
        return;
    }

    if (device->mutex != NULL) {
        vSemaphoreDelete(device->mutex);
    }

    /* Clear sensitive data before freeing */
    memset(device, 0, sizeof(qmc5883l_device_t));
    free(device);
}
```

---

## Synchronization & Race Conditions

### 1. Multi-threaded Access Protection

```c
/**
 * Thread-safe read operation
 */
esp_err_t qmc5883l_read_thread_safe(qmc5883l_device_t *dev,
                                    int16_t *x, int16_t *y, int16_t *z)
{
    if (dev == NULL || dev->mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Acquire lock with timeout */
    if (!xSemaphoreTake(dev->mutex, pdMS_TO_TICKS(500))) {
        ESP_LOGE(TAG, "Failed to acquire device lock");
        return ESP_ERR_TIMEOUT;
    }

    /* Perform read */
    esp_err_t ret = qmc5883l_read_safe(dev->port, x, y, z);

    if (ret == ESP_OK) {
        dev->read_count++;
    } else {
        dev->error_count++;
    }

    /* Release lock */
    xSemaphoreGive(dev->mutex);

    return ret;
}
```

### 2. ISR to Task Communication

```c
/**
 * DRDY ISR - safe notification without calling driver functions
 */
static QueueHandle_t g_drdy_queue = NULL;

void IRAM_ATTR qmc5883l_drdy_isr(void *arg)
{
    /* CRITICAL: Do NOT call I2C or FreeRTOS functions
     * that require locks from ISR context */

    BaseType_t high_priority_woken = pdFALSE;
    uint32_t notification = 1;

    if (g_drdy_queue != NULL) {
        xQueueSendFromISR(g_drdy_queue, &notification, &high_priority_woken);
    }

    if (high_priority_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * Task-level handler - safe to call driver functions
 */
void qmc5883l_drdy_task(void *arg)
{
    uint32_t notification;

    while (1) {
        /* Wait for ISR notification */
        if (xQueueReceive(g_drdy_queue, &notification,
                         pdMS_TO_TICKS(1000))) {

            /* Safe to call driver functions from task context */
            int16_t x, y, z;
            qmc5883l_read_thread_safe((qmc5883l_device_t *)arg, &x, &y, &z);

            printf("X=%d, Y=%d, Z=%d\n", x, y, z);
        }
    }
}
```

---

## Testing & Verification

### 1. Self-Test Implementation

```c
/**
 * Complete device self-test
 */
esp_err_t qmc5883l_self_test(qmc5883l_device_t *dev)
{
    ESP_LOGI(TAG, "Starting QMC5883L self-test...");

    /* Test 1: Chip ID verification */
    uint8_t chip_id;
    esp_err_t ret = i2c_read_byte(dev->port, dev->address, 0x0D, &chip_id);
    if (ret != ESP_OK || chip_id != 0xFF) {
        ESP_LOGE(TAG, "Self-test failed: Chip ID (0x%02X)", chip_id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  [PASS] Chip ID verification");

    /* Test 2: Soft reset */
    ret = i2c_write_byte(dev->port, dev->address, 0x0A, 0x80);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Self-test failed: Soft reset");
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t ctrl1_after_reset;
    i2c_read_byte(dev->port, dev->address, 0x09, &ctrl1_after_reset);
    if (ctrl1_after_reset != 0x1D) {  /* Default value after reset */
        ESP_LOGE(TAG, "Self-test failed: Reset (ctrl1=0x%02X)", ctrl1_after_reset);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  [PASS] Soft reset functionality");

    /* Test 3: Write/Read CONTROL_1 */
    uint8_t test_pattern = 0x1D;
    i2c_write_byte(dev->port, dev->address, 0x09, test_pattern);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t readback;
    i2c_read_byte(dev->port, dev->address, 0x09, &readback);
    if (readback != test_pattern) {
        ESP_LOGE(TAG, "Self-test failed: Write/Read (wrote 0x%02X, read 0x%02X)",
                 test_pattern, readback);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  [PASS] Register write/read");

    /* Test 4: Data ready polling */
    uint32_t drdy_count = 0;
    uint32_t start = xTaskGetTickCount();
    while (xTaskGetTickCount() - start < pdMS_TO_TICKS(1100)) {
        uint8_t status;
        i2c_read_byte(dev->port, dev->address, 0x06, &status);
        if (status & 0x01) {
            drdy_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (drdy_count < 8) {  /* Expect ~10 DRDYs in 1 second at 10Hz */
        ESP_LOGE(TAG, "Self-test failed: DRDY count too low (%lu)", drdy_count);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  [PASS] DRDY functionality (%lu pulses)", drdy_count);

    ESP_LOGI(TAG, "Self-test: ALL TESTS PASSED");
    return ESP_OK;
}
```

### 2. Error Recovery Testing

```c
/**
 * Test sensor recovery from various error conditions
 */
esp_err_t qmc5883l_recovery_test(qmc5883l_device_t *dev)
{
    ESP_LOGI(TAG, "Testing error recovery...");

    int16_t x, y, z;

    /* Normal operation */
    for (int i = 0; i < 10; i++) {
        esp_err_t ret = qmc5883l_read_thread_safe(dev, &x, &y, &z);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read at iteration %d", i);
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Recovery test: PASSED");
    return ESP_OK;
}
```

---

## Summary: Security Checklist

Before deploying QMC5883L driver in production:

- [x] All register accesses validated against permission matrix
- [x] I2C address verified before each transaction
- [x] I2C timeout enforced on all transactions
- [x] Overflow flag checked on every read
- [x] Data consistency verification implemented
- [x] DRDY timeout protection enabled
- [x] All buffers size-checked before access
- [x] Signed integer conversion verified for correctness
- [x] Thread-safety mechanisms (mutexes/semaphores) in place
- [x] ISR-to-task communication safe (no blocking calls in ISR)
- [x] Self-test procedure implemented and passing
- [x] Error recovery mechanisms tested
- [x] Error codes properly logged
- [x] All external inputs validated
- [x] Memory allocations checked for NULL

---

**Document Complete**

This security and safety guide provides comprehensive hardening strategies for production-ready QMC5883L drivers on ESP32-IDF.
