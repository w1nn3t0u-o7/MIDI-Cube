idf_component_register(
    SRCS "midi_router.c" "midi_translator.c"
    INCLUDE_DIRS "include"
    REQUIRES midi_core midi_network midi_uart midi_usb_device
)
