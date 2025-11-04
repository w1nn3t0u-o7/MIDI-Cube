idf_component_register(
    SRCS "midi_wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash
)


