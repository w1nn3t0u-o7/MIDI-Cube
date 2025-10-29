idf_component_register(
    SRCS 
        "midi_wifi.c"
        "midi_wifi_discovery.c"
        "midi_wifi_session.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        midi_core
        esp_wifi           # WiFi driver
        esp_netif          # Network interface
        esp_event          # Event loop
        mdns               # mDNS for discovery
        lwip               # UDP sockets
        nvs_flash          # For WiFi credentials
        esp_timer
    PRIV_REQUIRES
        freertos
)

