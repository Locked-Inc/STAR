idf_component_register(
    SRCS "src/star_camera_imx219.c"
    INCLUDE_DIRS "include"
    REQUIRES star_bus star_error_handler
)
