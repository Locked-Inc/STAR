#include <unity.h>
#include <string.h>
#include "esp_log.h"
#include "star_bms_bq7850.h"
#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_error_handler.h"
#include "driver/gpio.h"

/* Test configuration */
#define TEST_I2C_SDA_PIN (GPIO_NUM_21)
#define TEST_I2C_SCL_PIN (GPIO_NUM_22)
#define TEST_I2C_CLOCK (100000)

static star_bus_manager_t test_manager;

/* Test fixture setup */
void setUp(void)
{
    /* Initialize bus manager */
    star_bus_manager_init(&test_manager, "BMS_Test");
}

/* Test fixture teardown */
void tearDown(void)
{
    star_bus_manager_deinit(&test_manager);
}

/* --- BQ7850 Tests --- */

void test_bq7850_config_structure_initialization(void)
{
    bq7850_config_t config = {
        .num_cells       = 4,
        .num_temp        = 1,
        .smbus_addr      = BQ7850_DEFAULT_ADDR,
        .design_capacity = 2500,
        .design_voltage  = 14800,
    };

    TEST_ASSERT_EQUAL(4, config.num_cells);
    TEST_ASSERT_EQUAL(1, config.num_temp);
    TEST_ASSERT_EQUAL(BQ7850_DEFAULT_ADDR, config.smbus_addr);
    TEST_ASSERT_EQUAL(2500, config.design_capacity);
    TEST_ASSERT_EQUAL(14800, config.design_voltage);
}

void test_bq7850_invalid_cell_count_rejected(void)
{
    bq7850_config_t config = {
        .num_cells       = 0, /* Invalid */
        .num_temp        = 1,
        .smbus_addr      = BQ7850_DEFAULT_ADDR,
        .design_capacity = 2500,
        .design_voltage  = 14800,
    };

    TEST_ASSERT_EQUAL(0, config.num_cells);

    config.num_cells = 17; /* Over max */
    TEST_ASSERT_GREATER_THAN(BQ7850_MAX_CELLS, config.num_cells);
}

void test_bq7850_cell_data_structure(void)
{
    bq7850_cell_data_t cell_data;
    memset(&cell_data, 0, sizeof(cell_data));

    TEST_ASSERT_EQUAL(0, cell_data.valid_cells);
    TEST_ASSERT_EQUAL(0, cell_data.pack_mv);

    /* Simulate cell readings */
    cell_data.cell_mv[0]  = 3700;
    cell_data.cell_mv[1]  = 3710;
    cell_data.cell_mv[2]  = 3705;
    cell_data.cell_mv[3]  = 3715;
    cell_data.valid_cells = 4;
    cell_data.pack_mv     = 14830;

    TEST_ASSERT_EQUAL(4, cell_data.valid_cells);
    TEST_ASSERT_EQUAL(3700, cell_data.cell_mv[0]);
    TEST_ASSERT_EQUAL(14830, cell_data.pack_mv);
}

void test_bq7850_temperature_data_structure(void)
{
    bq7850_temp_data_t temp_data;
    memset(&temp_data, 0, sizeof(temp_data));

    /* Simulate temperature readings (in 0.1C) */
    temp_data.temp_c[0]     = 250; /* 25.0C */
    temp_data.temp_c[1]     = 260; /* 26.0C */
    temp_data.valid_sensors = 2;
    temp_data.avg_temp_c    = 255; /* 25.5C */

    TEST_ASSERT_EQUAL(2, temp_data.valid_sensors);
    TEST_ASSERT_EQUAL(250, temp_data.temp_c[0]);
    TEST_ASSERT_EQUAL(255, temp_data.avg_temp_c);
}

void test_bq7850_current_data_structure(void)
{
    bq7850_current_data_t current_data;
    memset(&current_data, 0, sizeof(current_data));

    /* Simulate charging scenario */
    current_data.current_ma     = 1500; /* 1.5A charging */
    current_data.avg_current_ma = 1450;
    current_data.voltage_mv     = 14800;
    current_data.power_mw       = 22200; /* ~22W */

    TEST_ASSERT_EQUAL(1500, current_data.current_ma);
    TEST_ASSERT_EQUAL(14800, current_data.voltage_mv);
    TEST_ASSERT_EQUAL(22200, current_data.power_mw);
}

void test_bq7850_soc_data_structure(void)
{
    bq7850_soc_data_t soc_data;
    memset(&soc_data, 0, sizeof(soc_data));

    soc_data.remaining_capacity_mah = 2000;
    soc_data.full_capacity_mah      = 2500;
    soc_data.relative_soc           = 80;
    soc_data.absolute_soc           = 75;
    soc_data.cycle_count            = 42;

    TEST_ASSERT_EQUAL(2000, soc_data.remaining_capacity_mah);
    TEST_ASSERT_EQUAL(80, soc_data.relative_soc);
    TEST_ASSERT_EQUAL(42, soc_data.cycle_count);
}

void test_bq7850_status_flags(void)
{
    bq7850_status_t status;
    memset(&status, 0, sizeof(status));

    /* Simulate charging state */
    status.battery_status = 0;
    status.safety_status  = 0;
    status.charging       = true;
    status.discharging    = false;
    status.fully_charged  = false;
    status.fault_active   = false;

    TEST_ASSERT_TRUE(status.charging);
    TEST_ASSERT_FALSE(status.discharging);
    TEST_ASSERT_FALSE(status.fault_active);
}

void test_bq7850_temperature_conversion(void)
{
    /* Test conversion from 0.1K to Celsius */
    int16_t temp_01k;
    float   temp_c;

    /* 25.0C = 298.15K = 2981 in 0.1K units */
    temp_01k = 2981;
    temp_c   = star_bms_bq7850_convert_temp_to_celsius(temp_01k);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 25.0, temp_c);

    /* 0.0C = 273.15K = 2731 in 0.1K units */
    temp_01k = 2731;
    temp_c   = star_bms_bq7850_convert_temp_to_celsius(temp_01k);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0, temp_c);

    /* -10.0C = 263.15K = 2631 in 0.1K units */
    temp_01k = 2631;
    temp_c   = star_bms_bq7850_convert_temp_to_celsius(temp_01k);
    TEST_ASSERT_FLOAT_WITHIN(0.1, -10.0, temp_c);
}

void test_bq7850_fault_detection(void)
{
    bq7850_status_t status;

    /* No faults */
    memset(&status, 0, sizeof(status));
    status.safety_status = 0;
    status.alarm_warning = 0;
    TEST_ASSERT_FALSE(star_bms_bq7850_is_fault_active(&status));

    /* Cell undervoltage fault */
    status.safety_status = BQ7850_SAFETY_STATUS_CUV;
    TEST_ASSERT_TRUE(star_bms_bq7850_is_fault_active(&status));

    /* Overcurrent fault */
    status.safety_status = BQ7850_SAFETY_STATUS_OCC;
    TEST_ASSERT_TRUE(star_bms_bq7850_is_fault_active(&status));

    /* Alarm/warning active */
    status.safety_status = 0;
    status.alarm_warning = 0x0001;
    TEST_ASSERT_TRUE(star_bms_bq7850_is_fault_active(&status));
}

void test_bq7850_status_string_generation(void)
{
    bq7850_status_t status;
    char            buffer[256];

    memset(&status, 0, sizeof(status));
    status.charging      = true;
    status.discharging   = false;
    status.fully_charged = false;
    status.safety_status = 0;

    size_t len = star_bms_bq7850_status_to_string(&status, buffer, sizeof(buffer));

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "CHARGING"));
}

void test_bq7850_protection_structure(void)
{
    bq7850_protection_t protection;
    memset(&protection, 0, sizeof(protection));

    /* Set typical protection thresholds for Li-ion cells */
    protection.overvoltage_mv   = 4200;  /* 4.2V per cell */
    protection.undervoltage_mv  = 3000;  /* 3.0V per cell */
    protection.overcharge_ma    = 5000;  /* 5A */
    protection.overdischarge_ma = 10000; /* 10A */
    protection.overtemp_c       = 600;   /* 60C */
    protection.undertemp_c      = -100;  /* -10C */

    TEST_ASSERT_EQUAL(4200, protection.overvoltage_mv);
    TEST_ASSERT_EQUAL(3000, protection.undervoltage_mv);
    TEST_ASSERT_EQUAL(5000, protection.overcharge_ma);
    TEST_ASSERT_EQUAL(600, protection.overtemp_c);
}

void test_bq7850_complete_battery_state(void)
{
    bq7850_battery_state_t state;
    memset(&state, 0, sizeof(state));

    /* Populate complete battery state */
    state.cells.valid_cells = 4;
    state.cells.pack_mv     = 14800;

    state.temps.valid_sensors = 1;
    state.temps.avg_temp_c    = 250;

    state.current.current_ma = 500;
    state.current.voltage_mv = 14800;
    state.current.power_mw   = 7400;

    state.soc.relative_soc = 80;
    state.soc.cycle_count  = 10;

    state.status.charging     = true;
    state.status.fault_active = false;

    /* Verify state */
    TEST_ASSERT_EQUAL(4, state.cells.valid_cells);
    TEST_ASSERT_EQUAL(250, state.temps.avg_temp_c);
    TEST_ASSERT_EQUAL(500, state.current.current_ma);
    TEST_ASSERT_EQUAL(80, state.soc.relative_soc);
    TEST_ASSERT_TRUE(state.status.charging);
}

void test_bq7850_device_info_structure(void)
{
    bq7850_device_info_t info;
    memset(&info, 0, sizeof(info));

    info.device_type   = 0x7850;
    info.fw_version    = 0x0100;
    info.hw_version    = 0x0001;
    info.serial_number = 12345678;

    strncpy(info.manufacturer, "Texas Instruments", sizeof(info.manufacturer) - 1);
    strncpy(info.device_name, "BQ7850", sizeof(info.device_name) - 1);
    strncpy(info.chemistry, "LION", sizeof(info.chemistry) - 1);

    TEST_ASSERT_EQUAL(0x7850, info.device_type);
    TEST_ASSERT_EQUAL(0x0100, info.fw_version);
    TEST_ASSERT_EQUAL_STRING("Texas Instruments", info.manufacturer);
    TEST_ASSERT_EQUAL_STRING("BQ7850", info.device_name);
}

void test_bq7850_cell_balancing_mask(void)
{
    uint16_t cell_mask;

    /* Enable balancing for cells 1 and 3 */
    cell_mask = (1 << 0) | (1 << 2); /* Bits 0 and 2 */
    TEST_ASSERT_EQUAL(0x0005, cell_mask);

    /* Enable balancing for all 4 cells */
    cell_mask = 0x000F; /* Bits 0-3 */
    TEST_ASSERT_EQUAL(0x000F, cell_mask);

    /* Disable all balancing */
    cell_mask = 0x0000;
    TEST_ASSERT_EQUAL(0x0000, cell_mask);
}

void test_bq7850_register_addresses(void)
{
    TEST_ASSERT_EQUAL(0x00, BQ7850_CMD_MANUFACTURER_ACCESS);
    TEST_ASSERT_EQUAL(0x09, BQ7850_CMD_VOLTAGE);
    TEST_ASSERT_EQUAL(0x0A, BQ7850_CMD_CURRENT);
    TEST_ASSERT_EQUAL(0x0D, BQ7850_CMD_RELATIVE_STATE_CHARGE);
    TEST_ASSERT_EQUAL(0x16, BQ7850_CMD_BATTERY_STATUS);
    TEST_ASSERT_EQUAL(0x51, BQ7850_CMD_SAFETY_STATUS);
}

void test_bq7850_subcommand_addresses(void)
{
    TEST_ASSERT_EQUAL(0x0001, BQ7850_SUBCMD_DEVICE_TYPE);
    TEST_ASSERT_EQUAL(0x0002, BQ7850_SUBCMD_FW_VERSION);
    TEST_ASSERT_EQUAL(0x0041, BQ7850_SUBCMD_RESET);
    TEST_ASSERT_EQUAL(0x0070, BQ7850_SUBCMD_CELL_VOLTAGE_BASE);
    TEST_ASSERT_EQUAL(0x0078, BQ7850_SUBCMD_TEMP_SENSOR_BASE);
}

void test_bq7850_safety_status_flags(void)
{
    uint16_t safety_status = 0;

    /* Test individual flag bits */
    safety_status |= BQ7850_SAFETY_STATUS_CUV;
    TEST_ASSERT_TRUE(safety_status & BQ7850_SAFETY_STATUS_CUV);

    safety_status |= BQ7850_SAFETY_STATUS_COV;
    TEST_ASSERT_TRUE(safety_status & BQ7850_SAFETY_STATUS_COV);

    safety_status |= BQ7850_SAFETY_STATUS_OCC;
    TEST_ASSERT_TRUE(safety_status & BQ7850_SAFETY_STATUS_OCC);

    /* Test clearing flags */
    safety_status &= ~BQ7850_SAFETY_STATUS_CUV;
    TEST_ASSERT_FALSE(safety_status & BQ7850_SAFETY_STATUS_CUV);
}

void test_bq7850_battery_status_flags(void)
{
    uint16_t battery_status = 0;

    /* Test discharging flag */
    battery_status |= BQ7850_BATTERY_STATUS_DSG;
    TEST_ASSERT_TRUE(battery_status & BQ7850_BATTERY_STATUS_DSG);

    /* Test fully charged flag */
    battery_status |= BQ7850_BATTERY_STATUS_FC;
    TEST_ASSERT_TRUE(battery_status & BQ7850_BATTERY_STATUS_FC);

    /* Test initialized flag */
    battery_status |= BQ7850_BATTERY_STATUS_INIT;
    TEST_ASSERT_TRUE(battery_status & BQ7850_BATTERY_STATUS_INIT);
}

void test_bq7850_null_pointer_handling(void)
{
    /* These should return ESP_ERR_INVALID_ARG */
    uint16_t voltage;
    bq7850_handle_t handle;
    memset(&handle, 0, sizeof(handle));

    /* Test 1: NULL handle */
    esp_err_t ret = star_bms_bq7850_read_pack_voltage(NULL, &voltage);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    /* Test 2: NULL voltage pointer */
    ret = star_bms_bq7850_read_pack_voltage(&handle, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/* --- Error Handler Tests --- */

void test_bms_error_handler_initialized_on_handle_create(void)
{
    star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
    TEST_ASSERT_NOT_NULL(i2c_config);

    esp_err_t ret = star_bus_manager_add_bus(&test_manager, i2c_config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    bq7850_handle_t bms_handle;
    bq7850_config_t bms_config = {
        .num_cells       = 4,
        .num_temp        = 2,
        .smbus_addr      = BQ7850_DEFAULT_ADDR,
        .design_capacity = 5000,
        .design_voltage  = 14800,
    };

    ret = star_bms_bq7850_init(&bms_handle, &test_manager, "bms_i2c", &bms_config);

    if (ret == ESP_OK) {
        TEST_ASSERT_NOT_NULL(bms_handle.error_handler.mutex);
        TEST_ASSERT_EQUAL(0, bms_handle.error_handler.error_count);
        TEST_ASSERT_EQUAL(0, bms_handle.error_handler.current_retry);
        TEST_ASSERT_FALSE(bms_handle.error_handler.in_error_state);

        TEST_ASSERT_EQUAL(3, bms_handle.error_handler.max_retries);
        TEST_ASSERT_EQUAL(50, bms_handle.error_handler.base_retry_delay);
        TEST_ASSERT_EQUAL(200, bms_handle.error_handler.max_retry_delay);

        star_bms_bq7850_deinit(&bms_handle);
    }
}

void test_bms_error_handler_has_no_reset_function(void)
{
    star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
    TEST_ASSERT_NOT_NULL(i2c_config);
    star_bus_manager_add_bus(&test_manager, i2c_config);

    bq7850_handle_t bms_handle;
    bq7850_config_t bms_config = {
        .num_cells       = 4,
        .num_temp        = 2,
        .smbus_addr      = BQ7850_DEFAULT_ADDR,
        .design_capacity = 5000,
        .design_voltage  = 14800,
    };

    esp_err_t ret = star_bms_bq7850_init(&bms_handle, &test_manager, "bms_i2c", &bms_config);

    if (ret == ESP_OK) {
        TEST_ASSERT_NULL(bms_handle.error_handler.reset_fn);
        TEST_ASSERT_NULL(bms_handle.error_handler.reset_context);
        star_bms_bq7850_deinit(&bms_handle);
    }
}

void test_bms_error_handler_deinitialized_on_handle_destroy(void)
{
    star_bus_config_t* i2c_config = star_bus_config_create_i2c("bms_i2c",
                                                             I2C_NUM_0,
                                                             BQ7850_DEFAULT_ADDR,
                                                             TEST_I2C_SDA_PIN,
                                                             TEST_I2C_SCL_PIN,
                                                             TEST_I2C_CLOCK);
    TEST_ASSERT_NOT_NULL(i2c_config);
    star_bus_manager_add_bus(&test_manager, i2c_config);

    bq7850_handle_t bms_handle;
    bq7850_config_t bms_config = {
        .num_cells       = 4,
        .num_temp        = 2,
        .smbus_addr      = BQ7850_DEFAULT_ADDR,
        .design_capacity = 5000,
        .design_voltage  = 14800,
    };

    esp_err_t ret = star_bms_bq7850_init(&bms_handle, &test_manager, "bms_i2c", &bms_config);

    if (ret == ESP_OK) {
        TEST_ASSERT_NOT_NULL(bms_handle.error_handler.mutex);
        ret = star_bms_bq7850_deinit(&bms_handle);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_NULL(bms_handle.manager);
        TEST_ASSERT_NULL(bms_handle.bus_name);
        TEST_ASSERT_EQUAL(0, bms_handle.smbus_addr);
    }
}

void app_main(void)
{
    UNITY_BEGIN();

    /* BQ7850 Tests */
    RUN_TEST(test_bq7850_config_structure_initialization);
    RUN_TEST(test_bq7850_invalid_cell_count_rejected);
    RUN_TEST(test_bq7850_cell_data_structure);
    RUN_TEST(test_bq7850_temperature_data_structure);
    RUN_TEST(test_bq7850_current_data_structure);
    RUN_TEST(test_bq7850_soc_data_structure);
    RUN_TEST(test_bq7850_status_flags);
    RUN_TEST(test_bq7850_temperature_conversion);
    RUN_TEST(test_bq7850_fault_detection);
    RUN_TEST(test_bq7850_status_string_generation);
    RUN_TEST(test_bq7850_protection_structure);
    RUN_TEST(test_bq7850_complete_battery_state);
    RUN_TEST(test_bq7850_device_info_structure);
    RUN_TEST(test_bq7850_cell_balancing_mask);
    RUN_TEST(test_bq7850_register_addresses);
    RUN_TEST(test_bq7850_subcommand_addresses);
    RUN_TEST(test_bq7850_safety_status_flags);
    RUN_TEST(test_bq7850_battery_status_flags);
    RUN_TEST(test_bq7850_null_pointer_handling);

    /* Error Handler Tests */
    RUN_TEST(test_bms_error_handler_initialized_on_handle_create);
    RUN_TEST(test_bms_error_handler_has_no_reset_function);
    RUN_TEST(test_bms_error_handler_deinitialized_on_handle_destroy);

    UNITY_END();
}
