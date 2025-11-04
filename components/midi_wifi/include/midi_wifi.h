#ifndef MIDI_WIFI_H
#define MIDI_WIFI_H

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

#define MIDI_WIFI_SSID      CONFIG_MIDI_WIFI_SSID
#define MIDI_WIFI_PASS      CONFIG_MIDI_WIFI_PASSWORD

typedef void (*wifi_connected_cb_t)(esp_netif_t *netif, const char *ip_addr);
typedef void (*wifi_disconnected_cb_t)(void);

typedef struct {
    EventGroupHandle_t wifi_event_group;
    esp_netif_t *wifi_netif;
    wifi_connected_cb_t connected_cb;
    wifi_disconnected_cb_t disconnected_cb;
    bool is_connected;
    uint8_t retry_num;
    uint8_t max_retry;           // Max connection retry attempts (0 = infinite)
} midi_wifi_config_t;

/**
 * @brief Initialize WiFi station mode
 * 
 * @param config WiFi configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t midi_wifi_init(void);

/**
 * @brief Register connection callbacks
 * 
 * @param connected_cb Called when WiFi connects and gets IP
 * @param disconnected_cb Called when WiFi disconnects
 */
void midi_wifi_register_callbacks(wifi_connected_cb_t connected_cb,
                                       wifi_disconnected_cb_t disconnected_cb);

/**
 * @brief Get WiFi network interface
 * 
 * @return esp_netif_t* Network interface or NULL if not initialized
 */
esp_netif_t* midi_wifi_get_netif(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected with valid IP
 */
bool midi_wifi_is_connected(void);

/**
 * @brief Disconnect and deinitialize WiFi
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t midi_wifi_deinit(void);

#endif /* MIDI_WIFI_H */

