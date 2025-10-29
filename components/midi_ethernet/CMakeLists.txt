idf_component_register(
    SRCS 
        "midi_ethernet.c"
        "midi_ethernet_w5500.c"
        "midi_ethernet_session.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        midi_core
        esp_eth           # ESP-IDF Ethernet driver
        esp_netif         # Network interface
        esp_event         # Event loop
        mdns              # mDNS for discovery
        lwip              # UDP sockets
        driver            # SPI driver
        esp_timer
    PRIV_REQUIRES
        freertos
)


