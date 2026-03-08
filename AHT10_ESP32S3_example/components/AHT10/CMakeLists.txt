# components/AHT10/CMakeLists.txt
idf_component_register(
    SRCS "aht10_sensor.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)