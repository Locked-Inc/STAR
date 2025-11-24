/**
 * @file main.c
 * @brief Multi-Directional HCSR04 Distance Sensors to PCA9685 Servo Control
 *
 * This example demonstrates using multiple HC-SR04 ultrasonic distance sensors
 * to control servo motors via the PCA9685 PWM driver. Each sensor monitors a
 * different direction and controls its own dedicated servo.
 *
 * Hardware Setup (Platform-Specific):
 * - 4x HC-SR04 sensors (Front, Back, Left, Right)
 * - PCA9685: I2C PWM controller at address 0x40
 * - 4x Servos: Connected to PCA9685 channels 0-3
 *
 * Platform-Specific GPIO Pins:
 *
 * ESP32-WROOM:
 * - I2C: SDA=GPIO21, SCL=GPIO22
 * - Front HC-SR04: Trigger=GPIO23, Echo=GPIO22 (repurposed)
 * - Back HC-SR04: Trigger=GPIO19, Echo=GPIO18
 * - Left HC-SR04: Trigger=GPIO5, Echo=GPIO17
 * - Right HC-SR04: Trigger=GPIO16, Echo=GPIO4
 *
 * ESP32-S3:
 * - I2C: SDA=GPIO8, SCL=GPIO9
 * - Front HC-SR04: Trigger=GPIO1, Echo=GPIO2
 * - Back HC-SR04: Trigger=GPIO3, Echo=GPIO4
 * - Left HC-SR04: Trigger=GPIO5, Echo=GPIO6
 * - Right HC-SR04: Trigger=GPIO7, Echo=GPIO15
 *
 * Behavior:
 * - Each sensor measures distance continuously (10-400cm range)
 * - Distance is mapped to servo angle: 10cm → 0°, 400cm → 180°
 * - All servos update concurrently every 200ms
 */

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <star_bus_config.h>
#include <star_bus_manager.h>
#include <star_error_handler.h>
#include <star_error_interface.h>
#include <star_pin_validator.h>
#include <star_sensor_hcsr04.h>
#include <star_sensor_pca9685.h>

/* Static file-scope TAG for logging */
static const char* s_TAG = "multi_hcsr04";

/* ========== Platform-Specific Pin Definitions ========== */

/* I2C pins - platform-specific via build flags */
#if defined(CONFIG_STAR_BOARD_ESP32_WROOM_32)
#define I2C_SDA_PIN (GPIO_NUM_21)
#define I2C_SCL_PIN (GPIO_NUM_22)
/* Front sensor */
#define HCSR04_FRONT_TRIG (GPIO_NUM_23)
#define HCSR04_FRONT_ECHO (GPIO_NUM_22)
/* Back sensor */
#define HCSR04_BACK_TRIG (GPIO_NUM_19)
#define HCSR04_BACK_ECHO (GPIO_NUM_18)
/* Left sensor */
#define HCSR04_LEFT_TRIG (GPIO_NUM_5)
#define HCSR04_LEFT_ECHO (GPIO_NUM_17)
/* Right sensor */
#define HCSR04_RIGHT_TRIG (GPIO_NUM_16)
#define HCSR04_RIGHT_ECHO (GPIO_NUM_4)
#elif defined(CONFIG_STAR_BOARD_ESP32_S3_WROOM_1_N16)
#define I2C_SDA_PIN (GPIO_NUM_8)
#define I2C_SCL_PIN (GPIO_NUM_9)
/* Front sensor */
#define HCSR04_FRONT_TRIG (GPIO_NUM_1)
#define HCSR04_FRONT_ECHO (GPIO_NUM_2)
/* Back sensor */
#define HCSR04_BACK_TRIG (GPIO_NUM_3)
#define HCSR04_BACK_ECHO (GPIO_NUM_4)
/* Left sensor */
#define HCSR04_LEFT_TRIG (GPIO_NUM_5)
#define HCSR04_LEFT_ECHO (GPIO_NUM_6)
/* Right sensor */
#define HCSR04_RIGHT_TRIG (GPIO_NUM_7)
#define HCSR04_RIGHT_ECHO (GPIO_NUM_15)
#else
#error                                                                                             \
  "Unsupported platform - define CONFIG_STAR_BOARD_ESP32_WROOM_32 or CONFIG_STAR_BOARD_ESP32_S3_WROOM_1_N16"
#endif

/* PCA9685 configuration */
#define PCA9685_I2C_ADDR (0x40)
#define PCA9685_PWM_FREQ (50)       /* 50Hz for standard servos */
#define PCA9685_I2C_CLK_HZ (100000) /* 100kHz I2C clock */

/* Servo channel assignments */
#define SERVO_CHANNEL_FRONT (0)
#define SERVO_CHANNEL_BACK (1)
#define SERVO_CHANNEL_LEFT (2)
#define SERVO_CHANNEL_RIGHT (3)

/* Distance to angle mapping parameters */
#define MIN_DISTANCE_CM (10.0f)
#define MAX_DISTANCE_CM (400.0f)
#define MIN_ANGLE (0)
#define MAX_ANGLE (180)

/* Update rate */
#define UPDATE_INTERVAL_MS (200)

/* Number of sensors/servos */
#define NUM_SENSORS (4)

/* Sensor direction enumeration */
typedef enum {
  SENSOR_FRONT = 0,
  SENSOR_BACK  = 1,
  SENSOR_LEFT  = 2,
  SENSOR_RIGHT = 3
} sensor_direction_t;

/* Sensor configuration structure */
typedef struct {
  const char*     name;
  gpio_num_t      trigger_pin;
  gpio_num_t      echo_pin;
  uint8_t         servo_channel;
  hcsr04_handle_t handle;
} sensor_config_t;

/**
 * @brief Map distance reading to servo angle (application-specific logic)
 *
 * @param[in] distance_cm Distance in centimeters (10-400cm)
 * @return Servo angle (0-180 degrees)
 */
static uint8_t map_distance_to_angle(float distance_cm)
{
  float clamped = distance_cm;

  /* Clamp distance to valid range */
  if (clamped < MIN_DISTANCE_CM) {
    clamped = MIN_DISTANCE_CM;
  }
  if (clamped > MAX_DISTANCE_CM) {
    clamped = MAX_DISTANCE_CM;
  }

  /* Linear mapping: closer objects → smaller angles */
  float   normalized = (clamped - MIN_DISTANCE_CM) / (MAX_DISTANCE_CM - MIN_DISTANCE_CM);
  uint8_t angle      = (uint8_t)((normalized * (MAX_ANGLE - MIN_ANGLE)) + MIN_ANGLE);

  return angle;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_TAG, "=== Multi-Directional HCSR04 to PCA9685 Servo Control ===");
  ESP_LOGI(s_TAG, "4-sensor obstacle avoidance system");

  /* ========== Setup Bus Manager ========== */

  /* Get pin validator interface */
  star_pin_interface_t pin_iface;
  pin_validator_get_interface(&pin_iface);

  /* Create error interface using helper */
  star_error_interface_t* error_iface = star_error_interface_create_default();
  if (error_iface == NULL) {
    ESP_LOGE(s_TAG, "Failed to create error interface");
    return;
  }

  /* Initialize bus manager */
  star_bus_manager_t bus_manager;
  ret = star_bus_manager_init(&bus_manager, "main_bus_mgr", error_iface, &pin_iface);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Bus manager init failed: %s", esp_err_to_name(ret));
    star_error_interface_destroy(error_iface);
    return;
  }

  /* ========== Setup Sensor Configurations ========== */

  sensor_config_t sensors[NUM_SENSORS] = {{.name          = "Front",
                                           .trigger_pin   = HCSR04_FRONT_TRIG,
                                           .echo_pin      = HCSR04_FRONT_ECHO,
                                           .servo_channel = SERVO_CHANNEL_FRONT},
                                          {.name          = "Back",
                                           .trigger_pin   = HCSR04_BACK_TRIG,
                                           .echo_pin      = HCSR04_BACK_ECHO,
                                           .servo_channel = SERVO_CHANNEL_BACK},
                                          {.name          = "Left",
                                           .trigger_pin   = HCSR04_LEFT_TRIG,
                                           .echo_pin      = HCSR04_LEFT_ECHO,
                                           .servo_channel = SERVO_CHANNEL_LEFT},
                                          {.name          = "Right",
                                           .trigger_pin   = HCSR04_RIGHT_TRIG,
                                           .echo_pin      = HCSR04_RIGHT_ECHO,
                                           .servo_channel = SERVO_CHANNEL_RIGHT}};

  /* ========== Setup HCSR04 Sensors ========== */

  for (int i = 0; i < NUM_SENSORS; i++) {
    /* Create GPIO bus for each sensor */
    gpio_num_t sensor_pins[] = {sensors[i].trigger_pin, sensors[i].echo_pin};
    char       bus_name[32];
    snprintf(bus_name, sizeof(bus_name), "hcsr04_%s_gpio", sensors[i].name);

    star_bus_config_t* sensor_bus = star_bus_config_create_gpio(bus_name, sensor_pins, 2);
    if (sensor_bus == NULL) {
      ESP_LOGE(s_TAG, "Failed to create %s sensor bus config", sensors[i].name);
      goto cleanup_bus_mgr;
    }

    ret = star_bus_manager_add_bus(&bus_manager, sensor_bus);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to add %s sensor bus: %s", sensors[i].name, esp_err_to_name(ret));
      star_bus_config_destroy(sensor_bus);
      goto cleanup_bus_mgr;
    }

    /* Initialize sensor */
    hcsr04_config_t hcsr04_cfg = {
      .trigger_pin   = sensors[i].trigger_pin,
      .echo_pin      = sensors[i].echo_pin,
      .temperature_c = 20.0f /* Room temperature for speed of sound calculation */
    };

    ret =
      star_sensor_hcsr04_init(&sensors[i].handle, &bus_manager, bus_name, error_iface, &hcsr04_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "%s HCSR04 init failed: %s", sensors[i].name, esp_err_to_name(ret));
      goto cleanup_bus_mgr;
    }

    ESP_LOGI(s_TAG,
             "✓ %s HCSR04 initialized (Trig: GPIO%d, Echo: GPIO%d)",
             sensors[i].name,
             sensors[i].trigger_pin,
             sensors[i].echo_pin);
  }

  /* ========== Setup PCA9685 Servo Controller ========== */

  /* Create I2C bus for PCA9685 */
  star_bus_config_t* i2c_bus = star_bus_config_create_i2c("pca9685_i2c",
                                                          I2C_NUM_0,
                                                          PCA9685_I2C_ADDR,
                                                          I2C_SDA_PIN,
                                                          I2C_SCL_PIN,
                                                          PCA9685_I2C_CLK_HZ);
  if (i2c_bus == NULL) {
    ESP_LOGE(s_TAG, "Failed to create I2C bus config");
    goto cleanup_bus_mgr;
  }

  ret = star_bus_manager_add_bus(&bus_manager, i2c_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to add I2C bus: %s", esp_err_to_name(ret));
    star_bus_config_destroy(i2c_bus);
    goto cleanup_bus_mgr;
  }

  /* Initialize PCA9685 */
  pca9685_handle_t servo_controller;
  pca9685_config_t pca9685_cfg = {.i2c_addr      = PCA9685_I2C_ADDR,
                                  .pwm_freq      = PCA9685_PWM_FREQ,
                                  .output_mode   = PCA9685_OUTPUT_TOTEM_POLE,
                                  .ext_clock     = false,
                                  .invert_output = false};

  ret = star_sensor_pca9685_init(&servo_controller,
                                 &bus_manager,
                                 "pca9685_i2c",
                                 error_iface,
                                 GPIO_NUM_NC, /* No output enable pin */
                                 false,       /* OE active low (not used) */
                                 &pca9685_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "PCA9685 init failed: %s", esp_err_to_name(ret));
    goto cleanup_bus_mgr;
  }

  ESP_LOGI(s_TAG,
           "✓ PCA9685 initialized (I2C: 0x%02X, PWM: %dHz)",
           PCA9685_I2C_ADDR,
           PCA9685_PWM_FREQ);
  ESP_LOGI(s_TAG, "✓ 4 Servos on channels 0-3 ready");

  /* ========== Main Control Loop ========== */

  ESP_LOGI(s_TAG, "Starting multi-sensor control loop...");
  ESP_LOGI(s_TAG,
           "Distance range: %.0f-%.0fcm → Angle range: %d-%d°",
           MIN_DISTANCE_CM,
           MAX_DISTANCE_CM,
           MIN_ANGLE,
           MAX_ANGLE);

  uint32_t loop_count = 0;

  while (1) {
    /* Read all sensors and update servos */
    for (int i = 0; i < NUM_SENSORS; i++) {
      float distance_cm;
      ret = star_sensor_hcsr04_read_distance(&sensors[i].handle, &distance_cm);

      if (ret == ESP_OK) {
        /* Valid reading - process it */
        uint8_t target_angle = map_distance_to_angle(distance_cm);

        /* Set servo position using library function */
        ret = star_sensor_pca9685_set_servo_angle(&servo_controller,
                                                  sensors[i].servo_channel,
                                                  target_angle);
        if (ret != ESP_OK) {
          ESP_LOGW(s_TAG,
                   "%s: Failed to set servo angle: %s",
                   sensors[i].name,
                   esp_err_to_name(ret));
        } else {
          /* Log every 10th successful update to avoid spam */
          if ((loop_count % 10) == 0) {
            ESP_LOGI(s_TAG,
                     "%s: %.1fcm → %d° (ch%d)",
                     sensors[i].name,
                     distance_cm,
                     target_angle,
                     sensors[i].servo_channel);
          }
        }
      } else {
        /* Sensor read failed - log warning but continue */
        if ((loop_count % 10) == 0) {
          ESP_LOGW(s_TAG, "%s: Sensor read failed: %s", sensors[i].name, esp_err_to_name(ret));
        }
      }
    }

    loop_count++;
    vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_MS));
  }

  /* ========== Cleanup (unreachable in this example) ========== */

  star_sensor_pca9685_deinit(&servo_controller);

  for (int i = 0; i < NUM_SENSORS; i++) {
    star_sensor_hcsr04_deinit(&sensors[i].handle);
  }

cleanup_bus_mgr:
  star_bus_manager_deinit(&bus_manager);
  star_error_interface_destroy(error_iface);

  ESP_LOGI(s_TAG, "Example terminated");
}
