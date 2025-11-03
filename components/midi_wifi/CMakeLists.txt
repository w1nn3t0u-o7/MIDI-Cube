idf_component_register(
    SRCS 
        "midi_wifi.c"
       
       
    INCLUDE_DIRS 
        "include"
    REQUIRES
        midi_core
        esp_wifi           # WiFi driver
        esp_netif          # Network interface
        esp_event          # Event loop
        lwip               # UDP sockets
        nvs_flash          # For WiFi credentials
        esp_timer
    PRIV_REQUIRES
        freertos
)

