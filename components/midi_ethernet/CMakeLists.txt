idf_component_register(
    SRCS "midi_ethernet.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_eth esp_netif esp_event esp_driver_gpio
)


