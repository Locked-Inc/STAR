/* MPU6050 6-Axis IMU Tests */

#include "unity.h"
#include "esp_err.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* MPU6050 I2C address */
#define MPU6050_ADDR_LOW            (0x68)
#define MPU6050_ADDR_HIGH           (0x69)

/* MPU6050 registers */
#define MPU6050_REG_WHO_AM_I        (0x75)
#define MPU6050_REG_PWR_MGMT_1      (0x6B)
#define MPU6050_REG_PWR_MGMT_2      (0x6C)
#define MPU6050_REG_CONFIG          (0x1A)
#define MPU6050_REG_GYRO_CONFIG     (0x1B)
#define MPU6050_REG_ACCEL_CONFIG    (0x1C)
#define MPU6050_REG_FIFO_EN         (0x23)
#define MPU6050_REG_INT_ENABLE      (0x38)
#define MPU6050_REG_INT_STATUS      (0x3A)
#define MPU6050_REG_ACCEL_XOUT_H    (0x3B)
#define MPU6050_REG_TEMP_OUT_H      (0x41)
#define MPU6050_REG_GYRO_XOUT_H     (0x43)
#define MPU6050_REG_USER_CTRL       (0x6A)
#define MPU6050_REG_FIFO_COUNT_H    (0x72)
#define MPU6050_REG_FIFO_R_W        (0x74)

/* WHO_AM_I expected value */
#define MPU6050_WHO_AM_I_VAL        (0x68)

/* Accelerometer ranges */
typedef enum {
    MPU6050_ACCEL_2G = 0,
    MPU6050_ACCEL_4G,
    MPU6050_ACCEL_8G,
    MPU6050_ACCEL_16G
} mpu6050_accel_range_t;

/* Gyroscope ranges */
typedef enum {
    MPU6050_GYRO_250DPS = 0,
    MPU6050_GYRO_500DPS,
    MPU6050_GYRO_1000DPS,
    MPU6050_GYRO_2000DPS
} mpu6050_gyro_range_t;

/* DLPF settings */
typedef enum {
    MPU6050_DLPF_260HZ = 0,
    MPU6050_DLPF_184HZ,
    MPU6050_DLPF_94HZ,
    MPU6050_DLPF_44HZ,
    MPU6050_DLPF_21HZ,
    MPU6050_DLPF_10HZ,
    MPU6050_DLPF_5HZ
} mpu6050_dlpf_t;

/* Error codes */
typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERR_INVALID_ARG,
    MPU6050_ERR_I2C_FAIL,
    MPU6050_ERR_WHO_AM_I,
    MPU6050_ERR_FIFO_OVERFLOW
} mpu6050_err_t;

/* Sensor data structures */
typedef struct {
    float x;
    float y;
    float z;
} mpu6050_vec3_t;

typedef struct {
    mpu6050_vec3_t accel;
    mpu6050_vec3_t gyro;
    float temperature;
} mpu6050_data_t;

/* Handle structure */
typedef struct {
    uint8_t i2c_addr;
    mpu6050_accel_range_t accel_range;
    mpu6050_gyro_range_t gyro_range;
    mpu6050_dlpf_t dlpf;
    float accel_sensitivity;
    float gyro_sensitivity;
    bool fifo_enabled;
    bool sleep_mode;
    bool initialized;
} mpu6050_handle_t;

/* Mock I2C state */
static struct {
    bool i2c_success;
    uint8_t who_am_i_response;
    uint8_t registers[128];
    int16_t accel_raw[3];
    int16_t gyro_raw[3];
    int16_t temp_raw;
    uint16_t fifo_count;
    bool fifo_overflow;
    int call_count;
} s_mock_i2c;

static mpu6050_handle_t s_test_handle;

/* Mock I2C functions */
static mpu6050_err_t priv_mock_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    if (!s_mock_i2c.i2c_success) return MPU6050_ERR_I2C_FAIL;
    s_mock_i2c.registers[reg] = value;
    s_mock_i2c.call_count++;
    return MPU6050_OK;
}

static mpu6050_err_t priv_mock_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *value) {
    if (!s_mock_i2c.i2c_success) return MPU6050_ERR_I2C_FAIL;
    if (reg == MPU6050_REG_WHO_AM_I) {
        *value = s_mock_i2c.who_am_i_response;
    } else if (reg == MPU6050_REG_INT_STATUS) {
        *value = s_mock_i2c.fifo_overflow ? 0x10 : 0x00;
    } else {
        *value = s_mock_i2c.registers[reg];
    }
    return MPU6050_OK;
}

static mpu6050_err_t priv_mock_i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    if (!s_mock_i2c.i2c_success) return MPU6050_ERR_I2C_FAIL;

    if (reg == MPU6050_REG_ACCEL_XOUT_H && len >= 14) {
        /* Accel X, Y, Z */
        data[0] = (s_mock_i2c.accel_raw[0] >> 8) & 0xFF;
        data[1] = s_mock_i2c.accel_raw[0] & 0xFF;
        data[2] = (s_mock_i2c.accel_raw[1] >> 8) & 0xFF;
        data[3] = s_mock_i2c.accel_raw[1] & 0xFF;
        data[4] = (s_mock_i2c.accel_raw[2] >> 8) & 0xFF;
        data[5] = s_mock_i2c.accel_raw[2] & 0xFF;
        /* Temperature */
        data[6] = (s_mock_i2c.temp_raw >> 8) & 0xFF;
        data[7] = s_mock_i2c.temp_raw & 0xFF;
        /* Gyro X, Y, Z */
        data[8] = (s_mock_i2c.gyro_raw[0] >> 8) & 0xFF;
        data[9] = s_mock_i2c.gyro_raw[0] & 0xFF;
        data[10] = (s_mock_i2c.gyro_raw[1] >> 8) & 0xFF;
        data[11] = s_mock_i2c.gyro_raw[1] & 0xFF;
        data[12] = (s_mock_i2c.gyro_raw[2] >> 8) & 0xFF;
        data[13] = s_mock_i2c.gyro_raw[2] & 0xFF;
    } else if (reg == MPU6050_REG_FIFO_COUNT_H && len >= 2) {
        data[0] = (s_mock_i2c.fifo_count >> 8) & 0xFF;
        data[1] = s_mock_i2c.fifo_count & 0xFF;
    }
    return MPU6050_OK;
}

/* Calculate sensitivities */
static float priv_get_accel_sensitivity(mpu6050_accel_range_t range) {
    switch (range) {
        case MPU6050_ACCEL_2G:  return 16384.0f;
        case MPU6050_ACCEL_4G:  return 8192.0f;
        case MPU6050_ACCEL_8G:  return 4096.0f;
        case MPU6050_ACCEL_16G: return 2048.0f;
        default: return 16384.0f;
    }
}

static float priv_get_gyro_sensitivity(mpu6050_gyro_range_t range) {
    switch (range) {
        case MPU6050_GYRO_250DPS:  return 131.0f;
        case MPU6050_GYRO_500DPS:  return 65.5f;
        case MPU6050_GYRO_1000DPS: return 32.8f;
        case MPU6050_GYRO_2000DPS: return 16.4f;
        default: return 131.0f;
    }
}

/* Simulated driver functions */
static mpu6050_err_t mpu6050_init(mpu6050_handle_t *handle, uint8_t i2c_addr) {
    if (handle == NULL) {
        return MPU6050_ERR_INVALID_ARG;
    }
    if (i2c_addr != MPU6050_ADDR_LOW && i2c_addr != MPU6050_ADDR_HIGH) {
        return MPU6050_ERR_INVALID_ARG;
    }

    /* Verify WHO_AM_I */
    uint8_t who_am_i;
    mpu6050_err_t err = priv_mock_i2c_read_reg(i2c_addr, MPU6050_REG_WHO_AM_I, &who_am_i);
    if (err != MPU6050_OK) return err;
    if (who_am_i != MPU6050_WHO_AM_I_VAL) {
        return MPU6050_ERR_WHO_AM_I;
    }

    memset(handle, 0, sizeof(mpu6050_handle_t));
    handle->i2c_addr = i2c_addr;
    handle->accel_range = MPU6050_ACCEL_2G;
    handle->gyro_range = MPU6050_GYRO_250DPS;
    handle->dlpf = MPU6050_DLPF_260HZ;
    handle->accel_sensitivity = priv_get_accel_sensitivity(handle->accel_range);
    handle->gyro_sensitivity = priv_get_gyro_sensitivity(handle->gyro_range);
    handle->fifo_enabled = false;
    handle->sleep_mode = false;

    /* Wake up from sleep (clear bit 6) */
    err = priv_mock_i2c_write_reg(i2c_addr, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (err != MPU6050_OK) return err;

    handle->initialized = true;
    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_set_accel_range(mpu6050_handle_t *handle, mpu6050_accel_range_t range) {
    if (handle == NULL || !handle->initialized) {
        return MPU6050_ERR_INVALID_ARG;
    }
    if (range > MPU6050_ACCEL_16G) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t config = (range << 3);
    mpu6050_err_t err = priv_mock_i2c_write_reg(handle->i2c_addr, MPU6050_REG_ACCEL_CONFIG, config);
    if (err != MPU6050_OK) return err;

    handle->accel_range = range;
    handle->accel_sensitivity = priv_get_accel_sensitivity(range);
    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_set_gyro_range(mpu6050_handle_t *handle, mpu6050_gyro_range_t range) {
    if (handle == NULL || !handle->initialized) {
        return MPU6050_ERR_INVALID_ARG;
    }
    if (range > MPU6050_GYRO_2000DPS) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t config = (range << 3);
    mpu6050_err_t err = priv_mock_i2c_write_reg(handle->i2c_addr, MPU6050_REG_GYRO_CONFIG, config);
    if (err != MPU6050_OK) return err;

    handle->gyro_range = range;
    handle->gyro_sensitivity = priv_get_gyro_sensitivity(range);
    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_set_dlpf(mpu6050_handle_t *handle, mpu6050_dlpf_t dlpf) {
    if (handle == NULL || !handle->initialized) {
        return MPU6050_ERR_INVALID_ARG;
    }
    if (dlpf > MPU6050_DLPF_5HZ) {
        return MPU6050_ERR_INVALID_ARG;
    }

    mpu6050_err_t err = priv_mock_i2c_write_reg(handle->i2c_addr, MPU6050_REG_CONFIG, dlpf);
    if (err != MPU6050_OK) return err;

    handle->dlpf = dlpf;
    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_read_accel(mpu6050_handle_t *handle, mpu6050_vec3_t *accel) {
    if (handle == NULL || !handle->initialized || accel == NULL) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t data[6];
    mpu6050_err_t err = priv_mock_i2c_read_bytes(handle->i2c_addr, MPU6050_REG_ACCEL_XOUT_H, data, 6);
    if (err != MPU6050_OK) return err;

    int16_t raw_x = (data[0] << 8) | data[1];
    int16_t raw_y = (data[2] << 8) | data[3];
    int16_t raw_z = (data[4] << 8) | data[5];

    accel->x = raw_x / handle->accel_sensitivity;
    accel->y = raw_y / handle->accel_sensitivity;
    accel->z = raw_z / handle->accel_sensitivity;

    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_read_gyro(mpu6050_handle_t *handle, mpu6050_vec3_t *gyro) {
    if (handle == NULL || !handle->initialized || gyro == NULL) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t data[14];  /* Read all to get gyro */
    mpu6050_err_t err = priv_mock_i2c_read_bytes(handle->i2c_addr, MPU6050_REG_ACCEL_XOUT_H, data, 14);
    if (err != MPU6050_OK) return err;

    int16_t raw_x = (data[8] << 8) | data[9];
    int16_t raw_y = (data[10] << 8) | data[11];
    int16_t raw_z = (data[12] << 8) | data[13];

    gyro->x = raw_x / handle->gyro_sensitivity;
    gyro->y = raw_y / handle->gyro_sensitivity;
    gyro->z = raw_z / handle->gyro_sensitivity;

    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_read_temp(mpu6050_handle_t *handle, float *temperature) {
    if (handle == NULL || !handle->initialized || temperature == NULL) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t data[14];
    mpu6050_err_t err = priv_mock_i2c_read_bytes(handle->i2c_addr, MPU6050_REG_ACCEL_XOUT_H, data, 14);
    if (err != MPU6050_OK) return err;

    int16_t raw_temp = (data[6] << 8) | data[7];
    *temperature = (raw_temp / 340.0f) + 36.53f;

    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_read_all(mpu6050_handle_t *handle, mpu6050_data_t *data) {
    if (handle == NULL || !handle->initialized || data == NULL) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t raw[14];
    mpu6050_err_t err = priv_mock_i2c_read_bytes(handle->i2c_addr, MPU6050_REG_ACCEL_XOUT_H, raw, 14);
    if (err != MPU6050_OK) return err;

    /* Accelerometer */
    int16_t raw_ax = (raw[0] << 8) | raw[1];
    int16_t raw_ay = (raw[2] << 8) | raw[3];
    int16_t raw_az = (raw[4] << 8) | raw[5];
    data->accel.x = raw_ax / handle->accel_sensitivity;
    data->accel.y = raw_ay / handle->accel_sensitivity;
    data->accel.z = raw_az / handle->accel_sensitivity;

    /* Temperature */
    int16_t raw_temp = (raw[6] << 8) | raw[7];
    data->temperature = (raw_temp / 340.0f) + 36.53f;

    /* Gyroscope */
    int16_t raw_gx = (raw[8] << 8) | raw[9];
    int16_t raw_gy = (raw[10] << 8) | raw[11];
    int16_t raw_gz = (raw[12] << 8) | raw[13];
    data->gyro.x = raw_gx / handle->gyro_sensitivity;
    data->gyro.y = raw_gy / handle->gyro_sensitivity;
    data->gyro.z = raw_gz / handle->gyro_sensitivity;

    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_fifo_enable(mpu6050_handle_t *handle, bool enable) {
    if (handle == NULL || !handle->initialized) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t user_ctrl = enable ? 0x44 : 0x00;  /* FIFO_EN and FIFO_RESET */
    mpu6050_err_t err = priv_mock_i2c_write_reg(handle->i2c_addr, MPU6050_REG_USER_CTRL, user_ctrl);
    if (err != MPU6050_OK) return err;

    if (enable) {
        /* Enable accel and gyro in FIFO */
        err = priv_mock_i2c_write_reg(handle->i2c_addr, MPU6050_REG_FIFO_EN, 0x78);
        if (err != MPU6050_OK) return err;
    }

    handle->fifo_enabled = enable;
    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_fifo_get_count(mpu6050_handle_t *handle, uint16_t *count) {
    if (handle == NULL || !handle->initialized || count == NULL) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t data[2];
    mpu6050_err_t err = priv_mock_i2c_read_bytes(handle->i2c_addr, MPU6050_REG_FIFO_COUNT_H, data, 2);
    if (err != MPU6050_OK) return err;

    *count = (data[0] << 8) | data[1];
    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_fifo_check_overflow(mpu6050_handle_t *handle, bool *overflow) {
    if (handle == NULL || !handle->initialized || overflow == NULL) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t status;
    mpu6050_err_t err = priv_mock_i2c_read_reg(handle->i2c_addr, MPU6050_REG_INT_STATUS, &status);
    if (err != MPU6050_OK) return err;

    *overflow = (status & 0x10) != 0;
    return MPU6050_OK;
}

static mpu6050_err_t mpu6050_set_sleep(mpu6050_handle_t *handle, bool sleep) {
    if (handle == NULL || !handle->initialized) {
        return MPU6050_ERR_INVALID_ARG;
    }

    uint8_t pwr_mgmt = sleep ? 0x40 : 0x00;
    mpu6050_err_t err = priv_mock_i2c_write_reg(handle->i2c_addr, MPU6050_REG_PWR_MGMT_1, pwr_mgmt);
    if (err != MPU6050_OK) return err;

    handle->sleep_mode = sleep;
    return MPU6050_OK;
}

/* Test setup/teardown */
void setUp(void) {
    memset(&mock_i2c, 0, sizeof(mock_i2c));
    memset(&test_handle, 0, sizeof(test_handle));
    mock_i2c.i2c_success = true;
    mock_i2c.who_am_i_response = MPU6050_WHO_AM_I_VAL;
    mock_i2c.accel_raw[2] = 16384;  /* 1g at 2G range */
    mock_i2c.temp_raw = 0;  /* ~36.53C */
}

void tearDown(void) {
}

/* === Initialization Tests === */

void test_init_null_handle(void) {
    mpu6050_err_t err = mpu6050_init(NULL, MPU6050_ADDR_LOW);
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, err);
}

void test_init_who_am_i_failure(void) {
    mock_i2c.who_am_i_response = 0x00;  /* Wrong value */
    mpu6050_err_t err = mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    TEST_ASSERT_EQUAL(MPU6050_ERR_WHO_AM_I, err);
}

void test_init_success(void) {
    mpu6050_err_t err = mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_TRUE(test_handle.initialized);
    TEST_ASSERT_EQUAL(MPU6050_ADDR_LOW, test_handle.i2c_addr);
    TEST_ASSERT_EQUAL(MPU6050_ACCEL_2G, test_handle.accel_range);
    TEST_ASSERT_EQUAL(MPU6050_GYRO_250DPS, test_handle.gyro_range);
}

void test_init_invalid_address(void) {
    mpu6050_err_t err = mpu6050_init(&test_handle, 0x50);
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, err);
}

void test_init_i2c_failure(void) {
    mock_i2c.i2c_success = false;
    mpu6050_err_t err = mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    TEST_ASSERT_EQUAL(MPU6050_ERR_I2C_FAIL, err);
}

/* === Configuration Tests === */

void test_set_accel_range_2g(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_accel_range(&test_handle, MPU6050_ACCEL_2G);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_EQUAL(MPU6050_ACCEL_2G, test_handle.accel_range);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 16384.0f, test_handle.accel_sensitivity);
}

void test_set_accel_range_4g(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_accel_range(&test_handle, MPU6050_ACCEL_4G);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8192.0f, test_handle.accel_sensitivity);
}

void test_set_accel_range_8g(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_accel_range(&test_handle, MPU6050_ACCEL_8G);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 4096.0f, test_handle.accel_sensitivity);
}

void test_set_accel_range_16g(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_accel_range(&test_handle, MPU6050_ACCEL_16G);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2048.0f, test_handle.accel_sensitivity);
}

void test_set_gyro_range_250dps(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_gyro_range(&test_handle, MPU6050_GYRO_250DPS);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 131.0f, test_handle.gyro_sensitivity);
}

void test_set_gyro_range_2000dps(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_gyro_range(&test_handle, MPU6050_GYRO_2000DPS);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 16.4f, test_handle.gyro_sensitivity);
}

void test_set_dlpf(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_dlpf(&test_handle, MPU6050_DLPF_44HZ);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_EQUAL(MPU6050_DLPF_44HZ, test_handle.dlpf);
}

void test_set_dlpf_invalid(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_dlpf(&test_handle, 10);
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, err);
}

/* === Reading Tests === */

void test_read_accel(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.accel_raw[0] = 0;
    mock_i2c.accel_raw[1] = 0;
    mock_i2c.accel_raw[2] = 16384;  /* 1g */

    mpu6050_vec3_t accel;
    mpu6050_err_t err = mpu6050_read_accel(&test_handle, &accel);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, accel.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, accel.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, accel.z);  /* 1g */
}

void test_read_gyro(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.gyro_raw[0] = 131;   /* 1 deg/s at 250dps */
    mock_i2c.gyro_raw[1] = 0;
    mock_i2c.gyro_raw[2] = -131;  /* -1 deg/s */

    mpu6050_vec3_t gyro;
    mpu6050_err_t err = mpu6050_read_gyro(&test_handle, &gyro);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, gyro.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, gyro.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -1.0f, gyro.z);
}

void test_read_temp(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.temp_raw = 0;  /* Should give ~36.53C */

    float temp;
    mpu6050_err_t err = mpu6050_read_temp(&test_handle, &temp);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 36.53f, temp);
}

void test_read_temp_high(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.temp_raw = 3400;  /* 10C above base */

    float temp;
    mpu6050_err_t err = mpu6050_read_temp(&test_handle, &temp);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 46.53f, temp);
}

void test_read_all_sensors(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.accel_raw[0] = 8192;   /* 0.5g at 2G */
    mock_i2c.accel_raw[1] = 0;
    mock_i2c.accel_raw[2] = 16384;  /* 1g */
    mock_i2c.gyro_raw[0] = 262;     /* 2 deg/s */
    mock_i2c.gyro_raw[1] = 0;
    mock_i2c.gyro_raw[2] = 0;
    mock_i2c.temp_raw = 0;

    mpu6050_data_t data;
    mpu6050_err_t err = mpu6050_read_all(&test_handle, &data);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, data.accel.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, data.accel.z);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.0f, data.gyro.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 36.53f, data.temperature);
}

/* === FIFO Tests === */

void test_fifo_enable(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_fifo_enable(&test_handle, true);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_TRUE(test_handle.fifo_enabled);
}

void test_fifo_disable(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_fifo_enable(&test_handle, true);
    mpu6050_err_t err = mpu6050_fifo_enable(&test_handle, false);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FALSE(test_handle.fifo_enabled);
}

void test_fifo_get_count(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.fifo_count = 512;

    uint16_t count;
    mpu6050_err_t err = mpu6050_fifo_get_count(&test_handle, &count);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_EQUAL(512, count);
}

void test_fifo_overflow_detection(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.fifo_overflow = true;

    bool overflow;
    mpu6050_err_t err = mpu6050_fifo_check_overflow(&test_handle, &overflow);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_TRUE(overflow);
}

void test_fifo_no_overflow(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.fifo_overflow = false;

    bool overflow;
    mpu6050_err_t err = mpu6050_fifo_check_overflow(&test_handle, &overflow);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FALSE(overflow);
}

/* === Power Management Tests === */

void test_sleep_mode_enable(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_err_t err = mpu6050_set_sleep(&test_handle, true);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_TRUE(test_handle.sleep_mode);
    TEST_ASSERT_EQUAL(0x40, mock_i2c.registers[MPU6050_REG_PWR_MGMT_1]);
}

void test_sleep_mode_disable(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mpu6050_set_sleep(&test_handle, true);
    mpu6050_err_t err = mpu6050_set_sleep(&test_handle, false);
    TEST_ASSERT_EQUAL(MPU6050_OK, err);
    TEST_ASSERT_FALSE(test_handle.sleep_mode);
    TEST_ASSERT_EQUAL(0x00, mock_i2c.registers[MPU6050_REG_PWR_MGMT_1]);
}

/* === Edge Cases === */

void test_sensitivity_calculation_2g(void) {
    float sens = priv_get_accel_sensitivity(MPU6050_ACCEL_2G);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 16384.0f, sens);
}

void test_sensitivity_calculation_16g(void) {
    float sens = priv_get_accel_sensitivity(MPU6050_ACCEL_16G);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2048.0f, sens);
}

void test_16bit_data_parsing_positive(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.accel_raw[0] = 32767;  /* Max positive */

    mpu6050_vec3_t accel;
    mpu6050_read_accel(&test_handle, &accel);
    TEST_ASSERT_TRUE(accel.x > 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, accel.x);  /* ~2g at 2G range */
}

void test_16bit_data_parsing_negative(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    mock_i2c.accel_raw[0] = -32768;  /* Max negative */

    mpu6050_vec3_t accel;
    mpu6050_read_accel(&test_handle, &accel);
    TEST_ASSERT_TRUE(accel.x < 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -2.0f, accel.x);  /* ~-2g at 2G range */
}

void test_read_null_params(void) {
    mpu6050_init(&test_handle, MPU6050_ADDR_LOW);
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, mpu6050_read_accel(&test_handle, NULL));
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, mpu6050_read_gyro(&test_handle, NULL));
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, mpu6050_read_temp(&test_handle, NULL));
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, mpu6050_read_all(&test_handle, NULL));
}

void test_config_uninitialized(void) {
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, mpu6050_set_accel_range(&test_handle, MPU6050_ACCEL_2G));
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, mpu6050_set_gyro_range(&test_handle, MPU6050_GYRO_250DPS));
    TEST_ASSERT_EQUAL(MPU6050_ERR_INVALID_ARG, mpu6050_set_dlpf(&test_handle, MPU6050_DLPF_44HZ));
}

/* Test runner */
int runUnityTests(void) {
    UNITY_BEGIN();

    /* Initialization Tests */
    RUN_TEST(test_init_null_handle);
    RUN_TEST(test_init_who_am_i_failure);
    RUN_TEST(test_init_success);
    RUN_TEST(test_init_invalid_address);
    RUN_TEST(test_init_i2c_failure);

    /* Configuration Tests */
    RUN_TEST(test_set_accel_range_2g);
    RUN_TEST(test_set_accel_range_4g);
    RUN_TEST(test_set_accel_range_8g);
    RUN_TEST(test_set_accel_range_16g);
    RUN_TEST(test_set_gyro_range_250dps);
    RUN_TEST(test_set_gyro_range_2000dps);
    RUN_TEST(test_set_dlpf);
    RUN_TEST(test_set_dlpf_invalid);

    /* Reading Tests */
    RUN_TEST(test_read_accel);
    RUN_TEST(test_read_gyro);
    RUN_TEST(test_read_temp);
    RUN_TEST(test_read_temp_high);
    RUN_TEST(test_read_all_sensors);

    /* FIFO Tests */
    RUN_TEST(test_fifo_enable);
    RUN_TEST(test_fifo_disable);
    RUN_TEST(test_fifo_get_count);
    RUN_TEST(test_fifo_overflow_detection);
    RUN_TEST(test_fifo_no_overflow);

    /* Power Management Tests */
    RUN_TEST(test_sleep_mode_enable);
    RUN_TEST(test_sleep_mode_disable);

    /* Edge Cases */
    RUN_TEST(test_sensitivity_calculation_2g);
    RUN_TEST(test_sensitivity_calculation_16g);
    RUN_TEST(test_16bit_data_parsing_positive);
    RUN_TEST(test_16bit_data_parsing_negative);
    RUN_TEST(test_read_null_params);
    RUN_TEST(test_config_uninitialized);

    return UNITY_END();
}

void app_main(void) {
    runUnityTests();
}
