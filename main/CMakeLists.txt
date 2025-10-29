idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES midi_core midi_uart midi_usb midi_wifi midi_ethernet
)