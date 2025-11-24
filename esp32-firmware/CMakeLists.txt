cmake_minimum_required(VERSION 3.16.0)
list(APPEND EXTRA_COMPONENT_DIRS
    "lib/star_bus"
    "lib/star_core"
    "lib/star_error_handler"
    "lib/star_pin_validator"
    "lib/star_sensor_mpu6050"
    "lib/star_sensor_bno055_bmp280"
    "lib/star_sensor_hcsr04"
    "lib/star_sensor_pca9685"
    "lib/star_bms_bq7850"
    "lib/star_servo"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(untitled)
