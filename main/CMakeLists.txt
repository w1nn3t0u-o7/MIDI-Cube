idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES midi_core midi_uart midi_translator midi_wifi midi_ethernet midi_network midi_usb_device
)