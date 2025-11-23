/* BH1750 Light Sensor Tests */

#include "unity.h"
#include "esp_err.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* BH1750 I2C address */
#define BH1750_ADDR_LOW             (0x23)
#define BH1750_ADDR_HIGH            (0x5C)

/* BH1750 commands */
#define BH1750_CMD_POWER_DOWN       (0x00)
#define BH1750_CMD_POWER_ON         (0x01)
#define BH1750_CMD_RESET            (0x07)
#define BH1750_CMD_CONT_H_RES       (0x10)
#define BH1750_CMD_CONT_H_RES2      (0x11)
#define BH1750_CMD_CONT_L_RES       (0x13)
#define BH1750_CMD_ONETIME_H_RES    (0x20)
#define BH1750_CMD_ONETIME_H_RES2   (0x21)
#define BH1750_CMD_ONETIME_L_RES    (0x23)
#define BH1750_CMD_MTREG_HIGH       (0x40)
#define BH1750_CMD_MTREG_LOW        (0x60)

/* MTreg limits */
#define BH1750_MTREG_MIN            (31)
#define BH1750_MTREG_MAX            (254)
#define BH1750_MTREG_DEFAULT        (69)

/* Resolution modes */
typedef enum {
    BH1750_MODE_LOW_RES = 0,
    BH1750_MODE_HIGH_RES,
    BH1750_MODE_HIGH_RES2
} bh1750_mode_t;

/* Error codes */
typedef enum {
    BH1750_OK = 0,
    BH1750_ERR_INVALID_ARG,
    BH1750_ERR_I2C_FAIL,
    BH1750_ERR_NOT_READY
} bh1750_err_t;

/* Handle structure */
typedef struct {
    uint8_t i2c_addr;
    uint8_t mtreg;
    bh1750_mode_t mode;
    bool continuous_mode;
    bool powered_on;
    bool initialized;
    uint16_t measurement_time_ms;
} bh1750_handle_t;

/* Mock I2C state */
static struct {
    bool i2c_success;
    uint8_t last_cmd;
    uint8_t last_addr;
    uint16_t raw_reading;
    int call_count;
    uint8_t cmd_history[16];
    int cmd_history_idx;
} s_mock_i2c;

static bh1750_handle_t s_s_test_handle;

/* Mock I2C functions */
static bh1750_err_t priv_mock_i2c_write_cmd(uint8_t addr, uint8_t cmd) {
    s_mock_i2c.last_addr = addr;
    s_mock_i2c.last_cmd = cmd;
    if (s_mock_i2c.cmd_history_idx < 16) {
        s_mock_i2c.cmd_history[s_mock_i2c.cmd_history_idx++] = cmd;
    }
    s_mock_i2c.call_count++;
    return s_mock_i2c.i2c_success ? BH1750_OK : BH1750_ERR_I2C_FAIL;
}

static bh1750_err_t priv_mock_i2c_read(uint8_t addr, uint8_t *data, size_t len) {
    if (!s_mock_i2c.i2c_success) return BH1750_ERR_I2C_FAIL;
    if (len >= 2) {
        data[0] = (s_mock_i2c.raw_reading >> 8) & 0xFF;
        data[1] = s_mock_i2c.raw_reading & 0xFF;
    }
    return BH1750_OK;
}

/* Calculate measurement time based on mode and MTreg */
static uint16_t priv_calculate_measurement_time(bh1750_mode_t mode, uint8_t mtreg) {
    uint16_t base_time;
    switch (mode) {
        case BH1750_MODE_LOW_RES:
            base_time = 24;  /* 24ms typical */
            break;
        case BH1750_MODE_HIGH_RES:
        case BH1750_MODE_HIGH_RES2:
        default:
            base_time = 180; /* 180ms typical */
            break;
    }
    /* Scale by MTreg ratio */
    return (base_time * mtreg) / BH1750_MTREG_DEFAULT;
}

/* Simulated driver functions */
static bh1750_err_t bh1750_init(bh1750_handle_t *handle, uint8_t i2c_addr, uint8_t mtreg) {
    if (handle == NULL) {
        return BH1750_ERR_INVALID_ARG;
    }
    if (i2c_addr != BH1750_ADDR_LOW && i2c_addr != BH1750_ADDR_HIGH) {
        return BH1750_ERR_INVALID_ARG;
    }
    if (mtreg < BH1750_MTREG_MIN || mtreg > BH1750_MTREG_MAX) {
        return BH1750_ERR_INVALID_ARG;
    }

    memset(handle, 0, sizeof(bh1750_handle_t));
    handle->i2c_addr = i2c_addr;
    handle->mtreg = mtreg;
    handle->mode = BH1750_MODE_HIGH_RES;
    handle->continuous_mode = true;
    handle->powered_on = false;
    handle->measurement_time_ms = priv_calculate_measurement_time(handle->mode, mtreg);

    /* Power on command */
    bh1750_err_t err = priv_mock_i2c_write_cmd(i2c_addr, BH1750_CMD_POWER_ON);
    if (err != BH1750_OK) return err;

    handle->powered_on = true;
    handle->initialized = true;

    return BH1750_OK;
}

static bh1750_err_t bh1750_set_mode(bh1750_handle_t *handle, bh1750_mode_t mode, bool continuous) {
    if (handle == NULL || !handle->initialized) {
        return BH1750_ERR_INVALID_ARG;
    }

    uint8_t cmd;
    if (continuous) {
        switch (mode) {
            case BH1750_MODE_LOW_RES:   cmd = BH1750_CMD_CONT_L_RES; break;
            case BH1750_MODE_HIGH_RES:  cmd = BH1750_CMD_CONT_H_RES; break;
            case BH1750_MODE_HIGH_RES2: cmd = BH1750_CMD_CONT_H_RES2; break;
            default: return BH1750_ERR_INVALID_ARG;
        }
    } else {
        switch (mode) {
            case BH1750_MODE_LOW_RES:   cmd = BH1750_CMD_ONETIME_L_RES; break;
            case BH1750_MODE_HIGH_RES:  cmd = BH1750_CMD_ONETIME_H_RES; break;
            case BH1750_MODE_HIGH_RES2: cmd = BH1750_CMD_ONETIME_H_RES2; break;
            default: return BH1750_ERR_INVALID_ARG;
        }
    }

    bh1750_err_t err = priv_mock_i2c_write_cmd(handle->i2c_addr, cmd);
    if (err != BH1750_OK) return err;

    handle->mode = mode;
    handle->continuous_mode = continuous;
    handle->measurement_time_ms = priv_calculate_measurement_time(mode, handle->mtreg);

    return BH1750_OK;
}

static bh1750_err_t bh1750_set_mtreg(bh1750_handle_t *handle, uint8_t mtreg) {
    if (handle == NULL || !handle->initialized) {
        return BH1750_ERR_INVALID_ARG;
    }
    if (mtreg < BH1750_MTREG_MIN || mtreg > BH1750_MTREG_MAX) {
        return BH1750_ERR_INVALID_ARG;
    }

    /* Write high bits */
    uint8_t high_cmd = BH1750_CMD_MTREG_HIGH | ((mtreg >> 5) & 0x07);
    bh1750_err_t err = priv_mock_i2c_write_cmd(handle->i2c_addr, high_cmd);
    if (err != BH1750_OK) return err;

    /* Write low bits */
    uint8_t low_cmd = BH1750_CMD_MTREG_LOW | (mtreg & 0x1F);
    err = priv_mock_i2c_write_cmd(handle->i2c_addr, low_cmd);
    if (err != BH1750_OK) return err;

    handle->mtreg = mtreg;
    handle->measurement_time_ms = priv_calculate_measurement_time(handle->mode, mtreg);

    return BH1750_OK;
}

static bh1750_err_t bh1750_power_down(bh1750_handle_t *handle) {
    if (handle == NULL || !handle->initialized) {
        return BH1750_ERR_INVALID_ARG;
    }

    bh1750_err_t err = priv_mock_i2c_write_cmd(handle->i2c_addr, BH1750_CMD_POWER_DOWN);
    if (err != BH1750_OK) return err;

    handle->powered_on = false;
    return BH1750_OK;
}

static bh1750_err_t bh1750_power_on(bh1750_handle_t *handle) {
    if (handle == NULL || !handle->initialized) {
        return BH1750_ERR_INVALID_ARG;
    }

    bh1750_err_t err = priv_mock_i2c_write_cmd(handle->i2c_addr, BH1750_CMD_POWER_ON);
    if (err != BH1750_OK) return err;

    handle->powered_on = true;
    return BH1750_OK;
}

static float raw_to_lux(uint16_t raw, bh1750_mode_t mode, uint8_t mtreg) {
    float lux = raw / 1.2f;  /* Base conversion */

    /* Adjust for MTreg */
    lux = lux * (BH1750_MTREG_DEFAULT / (float)mtreg);

    /* HIGH_RES2 mode has 0.5 lux resolution (divide by 2) */
    if (mode == BH1750_MODE_HIGH_RES2) {
        lux /= 2.0f;
    }

    return lux;
}

static bh1750_err_t bh1750_read(bh1750_handle_t *handle, float *lux) {
    if (handle == NULL || !handle->initialized || lux == NULL) {
        return BH1750_ERR_INVALID_ARG;
    }
    if (!handle->powered_on) {
        return BH1750_ERR_NOT_READY;
    }

    uint8_t data[2];
    bh1750_err_t err = priv_mock_i2c_read(handle->i2c_addr, data, 2);
    if (err != BH1750_OK) return err;

    uint16_t raw = (data[0] << 8) | data[1];
    *lux = raw_to_lux(raw, handle->mode, handle->mtreg);

    return BH1750_OK;
}

/* Test setup/teardown */
void setUp(void) {
    memset(&mock_i2c, 0, sizeof(mock_i2c));
    memset(&s_test_handle, 0, sizeof(s_test_handle));
    mock_i2c.i2c_success = true;
    mock_i2c.raw_reading = 1000;
}

void tearDown(void) {
}

/* === Initialization Tests === */

void test_init_null_handle(void) {
    bh1750_err_t err = bh1750_init(NULL, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    TEST_ASSERT_EQUAL(BH1750_ERR_INVALID_ARG, err);
}

void test_init_invalid_mtreg_low(void) {
    bh1750_err_t err = bh1750_init(&s_test_handle, BH1750_ADDR_LOW, 30);
    TEST_ASSERT_EQUAL(BH1750_ERR_INVALID_ARG, err);
}

void test_init_invalid_mtreg_high(void) {
    bh1750_err_t err = bh1750_init(&s_test_handle, BH1750_ADDR_LOW, 255);
    TEST_ASSERT_EQUAL(BH1750_ERR_INVALID_ARG, err);
}

void test_init_mtreg_boundary_min(void) {
    bh1750_err_t err = bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_MIN);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_EQUAL(BH1750_MTREG_MIN, s_test_handle.mtreg);
}

void test_init_mtreg_boundary_max(void) {
    bh1750_err_t err = bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_MAX);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_EQUAL(BH1750_MTREG_MAX, s_test_handle.mtreg);
}

void test_init_success(void) {
    bh1750_err_t err = bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_TRUE(s_test_handle.initialized);
    TEST_ASSERT_TRUE(s_test_handle.powered_on);
    TEST_ASSERT_EQUAL(BH1750_ADDR_LOW, s_test_handle.i2c_addr);
    TEST_ASSERT_EQUAL(BH1750_CMD_POWER_ON, mock_i2c.last_cmd);
}

void test_init_invalid_address(void) {
    bh1750_err_t err = bh1750_init(&s_test_handle, 0x50, BH1750_MTREG_DEFAULT);
    TEST_ASSERT_EQUAL(BH1750_ERR_INVALID_ARG, err);
}

void test_init_i2c_failure(void) {
    mock_i2c.i2c_success = false;
    bh1750_err_t err = bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    TEST_ASSERT_EQUAL(BH1750_ERR_I2C_FAIL, err);
}

/* === Configuration Tests === */

void test_set_mode_low_res(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    bh1750_err_t err = bh1750_set_mode(&s_test_handle, BH1750_MODE_LOW_RES, true);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_EQUAL(BH1750_MODE_LOW_RES, s_test_handle.mode);
    TEST_ASSERT_EQUAL(BH1750_CMD_CONT_L_RES, mock_i2c.last_cmd);
}

void test_set_mode_high_res(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    bh1750_err_t err = bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES, true);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_EQUAL(BH1750_MODE_HIGH_RES, s_test_handle.mode);
    TEST_ASSERT_EQUAL(BH1750_CMD_CONT_H_RES, mock_i2c.last_cmd);
}

void test_set_mode_high_res2(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    bh1750_err_t err = bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES2, true);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_EQUAL(BH1750_MODE_HIGH_RES2, s_test_handle.mode);
    TEST_ASSERT_EQUAL(BH1750_CMD_CONT_H_RES2, mock_i2c.last_cmd);
}

void test_measurement_time_low_res(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    bh1750_set_mode(&s_test_handle, BH1750_MODE_LOW_RES, true);

    /* Low res base is 24ms at default MTreg */
    TEST_ASSERT_EQUAL(24, s_test_handle.measurement_time_ms);
}

void test_measurement_time_high_res(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES, true);

    /* High res base is 180ms at default MTreg */
    TEST_ASSERT_EQUAL(180, s_test_handle.measurement_time_ms);
}

void test_measurement_time_with_mtreg(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, 138);  /* 2x default */
    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES, true);

    /* Should be approximately 2x base time */
    TEST_ASSERT_UINT16_WITHIN(10, 360, s_test_handle.measurement_time_ms);
}

void test_set_mtreg_valid(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    bh1750_err_t err = bh1750_set_mtreg(&s_test_handle, 100);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_EQUAL(100, s_test_handle.mtreg);
}

void test_set_mtreg_invalid_low(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    bh1750_err_t err = bh1750_set_mtreg(&s_test_handle, 10);
    TEST_ASSERT_EQUAL(BH1750_ERR_INVALID_ARG, err);
}

/* === Reading Tests === */

void test_read_raw_to_lux_conversion(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES, true);

    mock_i2c.raw_reading = 1200;  /* Should be 1000 lux (1200 / 1.2) */

    float lux;
    bh1750_err_t err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, lux);
}

void test_read_high_res2_conversion(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES2, true);

    mock_i2c.raw_reading = 1200;  /* Should be 500 lux (1200 / 1.2 / 2) */

    float lux;
    bh1750_err_t err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 500.0f, lux);
}

void test_read_zero_raw_value(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    mock_i2c.raw_reading = 0;

    float lux;
    bh1750_err_t err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lux);
}

void test_read_max_raw_value(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    mock_i2c.raw_reading = 65535;

    float lux;
    bh1750_err_t err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_TRUE(lux > 50000.0f);
}

void test_read_with_custom_mtreg(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, 138);  /* 2x default */
    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES, true);

    mock_i2c.raw_reading = 1200;  /* Base would be 1000 lux, but MTreg adjusts */

    float lux;
    bh1750_err_t err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    /* Lux should be halved due to 2x MTreg */
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 500.0f, lux);
}

/* === Power Management Tests === */

void test_power_down(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    TEST_ASSERT_TRUE(s_test_handle.powered_on);

    bh1750_err_t err = bh1750_power_down(&s_test_handle);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_FALSE(s_test_handle.powered_on);
    TEST_ASSERT_EQUAL(BH1750_CMD_POWER_DOWN, mock_i2c.last_cmd);
}

void test_power_on(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    bh1750_power_down(&s_test_handle);

    bh1750_err_t err = bh1750_power_on(&s_test_handle);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
    TEST_ASSERT_TRUE(s_test_handle.powered_on);
    TEST_ASSERT_EQUAL(BH1750_CMD_POWER_ON, mock_i2c.last_cmd);
}

void test_power_cycle(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    /* Power down */
    bh1750_power_down(&s_test_handle);
    TEST_ASSERT_FALSE(s_test_handle.powered_on);

    /* Read should fail when powered off */
    float lux;
    bh1750_err_t err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_ERR_NOT_READY, err);

    /* Power on and read should succeed */
    bh1750_power_on(&s_test_handle);
    err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_OK, err);
}

/* === Edge Cases === */

void test_mode_transition_continuous_to_onetime(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES, true);  /* Continuous */
    TEST_ASSERT_TRUE(s_test_handle.continuous_mode);

    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES, false);  /* One-time */
    TEST_ASSERT_FALSE(s_test_handle.continuous_mode);
    TEST_ASSERT_EQUAL(BH1750_CMD_ONETIME_H_RES, mock_i2c.last_cmd);
}

void test_onetime_mode_commands(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    bh1750_set_mode(&s_test_handle, BH1750_MODE_LOW_RES, false);
    TEST_ASSERT_EQUAL(BH1750_CMD_ONETIME_L_RES, mock_i2c.last_cmd);

    bh1750_set_mode(&s_test_handle, BH1750_MODE_HIGH_RES2, false);
    TEST_ASSERT_EQUAL(BH1750_CMD_ONETIME_H_RES2, mock_i2c.last_cmd);
}

void test_mtreg_encoding(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);
    mock_i2c.cmd_history_idx = 0;

    /* Set MTreg to 0b10101010 (170) */
    bh1750_set_mtreg(&s_test_handle, 170);

    /* High byte: 0x40 | (170 >> 5) = 0x40 | 5 = 0x45 */
    TEST_ASSERT_EQUAL(0x45, mock_i2c.cmd_history[0]);

    /* Low byte: 0x60 | (170 & 0x1F) = 0x60 | 10 = 0x6A */
    TEST_ASSERT_EQUAL(0x6A, mock_i2c.cmd_history[1]);
}

void test_null_lux_pointer(void) {
    bh1750_init(&s_test_handle, BH1750_ADDR_LOW, BH1750_MTREG_DEFAULT);

    bh1750_err_t err = bh1750_read(&s_test_handle, NULL);
    TEST_ASSERT_EQUAL(BH1750_ERR_INVALID_ARG, err);
}

void test_read_uninitialized(void) {
    float lux;
    bh1750_err_t err = bh1750_read(&s_test_handle, &lux);
    TEST_ASSERT_EQUAL(BH1750_ERR_INVALID_ARG, err);
}

/* Test runner */
int runUnityTests(void) {
    UNITY_BEGIN();

    /* Initialization Tests */
    RUN_TEST(test_init_null_handle);
    RUN_TEST(test_init_invalid_mtreg_low);
    RUN_TEST(test_init_invalid_mtreg_high);
    RUN_TEST(test_init_mtreg_boundary_min);
    RUN_TEST(test_init_mtreg_boundary_max);
    RUN_TEST(test_init_success);
    RUN_TEST(test_init_invalid_address);
    RUN_TEST(test_init_i2c_failure);

    /* Configuration Tests */
    RUN_TEST(test_set_mode_low_res);
    RUN_TEST(test_set_mode_high_res);
    RUN_TEST(test_set_mode_high_res2);
    RUN_TEST(test_measurement_time_low_res);
    RUN_TEST(test_measurement_time_high_res);
    RUN_TEST(test_measurement_time_with_mtreg);
    RUN_TEST(test_set_mtreg_valid);
    RUN_TEST(test_set_mtreg_invalid_low);

    /* Reading Tests */
    RUN_TEST(test_read_raw_to_lux_conversion);
    RUN_TEST(test_read_high_res2_conversion);
    RUN_TEST(test_read_zero_raw_value);
    RUN_TEST(test_read_max_raw_value);
    RUN_TEST(test_read_with_custom_mtreg);

    /* Power Management Tests */
    RUN_TEST(test_power_down);
    RUN_TEST(test_power_on);
    RUN_TEST(test_power_cycle);

    /* Edge Cases */
    RUN_TEST(test_mode_transition_continuous_to_onetime);
    RUN_TEST(test_onetime_mode_commands);
    RUN_TEST(test_mtreg_encoding);
    RUN_TEST(test_null_lux_pointer);
    RUN_TEST(test_read_uninitialized);

    return UNITY_END();
}

void app_main(void) {
    runUnityTests();
}
