idf_component_register(
    SRCS "midi_network.c"
    INCLUDE_DIRS "include"
    REQUIRES lwip esp_netif esp_event midi_core
)

