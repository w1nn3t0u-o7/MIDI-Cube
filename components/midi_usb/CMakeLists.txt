idf_component_register(
    SRCS 
        "midi_usb.c"
        "midi_usb_device.c"
        "midi_usb_host.c"
        "midi_usb_descriptors.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        midi_core
        driver
        esp_timer
        usb
        tinyusb
    PRIV_REQUIRES
        freertos
)

# Provide tusb_config.h to TinyUSB
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")


