# Main application component CMakeLists.txt

# Support for selecting example source files via STAR_EXAMPLE_SRC
# If STAR_EXAMPLE_SRC is set, use that file instead of main.c
if(DEFINED STAR_EXAMPLE_SRC)
    set(app_sources "${CMAKE_SOURCE_DIR}/${STAR_EXAMPLE_SRC}")
else()
    set(app_sources "${CMAKE_SOURCE_DIR}/src/main.c")
endif()

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES nvs_flash log
)
