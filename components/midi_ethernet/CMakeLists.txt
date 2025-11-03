idf_component_register(
    SRCS 
        "midi_ethernet.c"
       
       
    INCLUDE_DIRS 
        "include"
    REQUIRES
        midi_core
        esp_eth           # ESP-IDF Ethernet driver
        esp_netif         # Network interface
        esp_event         # Event loop
        lwip              # UDP sockets
        esp_driver_gpio   # GPIO driver
        esp_timer
    PRIV_REQUIRES
        freertos
)


