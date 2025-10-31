idf_component_register(
    SRCS "midi_uart.c" "midi_uart_config.c"
    INCLUDE_DIRS "include"
    REQUIRES midi_core esp_driver_uart esp_driver_gpio
)
