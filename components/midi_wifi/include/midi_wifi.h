/**
 * @file midi_wifi.h
 * @brief MIDI 2.0 over WiFi (UDP Transport)
 * 
 * Implements Network MIDI 2.0 using UDP transport over WiFi.
 * Based on MIDI Association spec M2-124-UM:
 * "User Datagram Protocol for Universal MIDI Packets"
 * 
 * Features:
 * - UMP (Universal MIDI Packet) transport over UDP
 * - Automatic device discovery via mDNS (DNS-SD)
 * - Session management (connect/disconnect)
 * - Forward Error Correction (FEC) optional
 * - Retransmit support for packet loss recovery
 * - Multiple simultaneous connections
 * - Low-latency streaming
 * 
 * Protocol Details:
 * - Port: 5004 (default host port)
 * - Service: _midi2._udp.local
 * - Payload: Raw UMP packets (4-16 bytes each)
 * - MTU: 1472 bytes max (to fit in single UDP packet)
 */

#ifndef MIDI_WIFI_H
#define MIDI_WIFI_H

#include "esp_err.h"
#include "esp_netif.h"

typedef struct {
    char ssid[32];
    char password[64];
    uint8_t max_retry;           // Max connection retry attempts (0 = infinite)
} wifi_transport_config_t;

typedef void (*wifi_connected_cb_t)(esp_netif_t *netif, const char *ip_addr);
typedef void (*wifi_disconnected_cb_t)(void);

/**
 * @brief Initialize WiFi station mode
 * 
 * @param config WiFi configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_transport_init(const wifi_transport_config_t *config);

/**
 * @brief Register connection callbacks
 * 
 * @param connected_cb Called when WiFi connects and gets IP
 * @param disconnected_cb Called when WiFi disconnects
 */
void wifi_transport_register_callbacks(wifi_connected_cb_t connected_cb,
                                       wifi_disconnected_cb_t disconnected_cb);

/**
 * @brief Get WiFi network interface
 * 
 * @return esp_netif_t* Network interface or NULL if not initialized
 */
esp_netif_t* wifi_transport_get_netif(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected with valid IP
 */
bool wifi_transport_is_connected(void);

/**
 * @brief Disconnect and deinitialize WiFi
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_transport_deinit(void);

#endif /* MIDI_WIFI_H */

