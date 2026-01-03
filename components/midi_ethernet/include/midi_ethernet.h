/**
 * @file midi_ethernet.h
 * @brief MIDI 2.0 over Ethernet (W5500 via SPI)
 * 
 * Implements Network MIDI 2.0 using W5500 Ethernet controller.
 * Based on MIDI Association spec M2-124-UM (same as WiFi).
 * 
 * Hardware:
 * - WIZnet W5500 Ethernet controller
 * - SPI interface to ESP32-S3
 * - 10/100 Mbps Ethernet PHY
 * - 32KB internal buffer
 * 
 * Features:
 * - UMP transport over UDP
 * - mDNS discovery (_midi2._udp.local)
 * - Session management
 * - Lower latency than WiFi (wired)
 * - Deterministic timing
 */

#ifndef MIDI_ETHERNET_H
#define MIDI_ETHERNET_H

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_driver.h"
#include "driver/spi_master.h"

typedef void (*eth_connected_cb_t)(esp_netif_t *netif, const char *ip_addr);
typedef void (*eth_disconnected_cb_t)(void);

typedef struct midi_eth_config {
    esp_netif_t *eth_netif;
    eth_connected_cb_t connected_cb;
    eth_disconnected_cb_t disconnected_cb;
    bool is_connected;
} midi_eth_config_t;

void midi_eth_register_callbacks(eth_connected_cb_t connected_cb,
                                          eth_disconnected_cb_t disconnected_cb);

/**
 * @brief Initialize Ethernet interface with configured IP mode
 * 
 * Uses Kconfig settings to determine:
 * - Static IP (CONFIG_MIDI_ETH_IP_STATIC) for direct PC connection
 * - DHCP (CONFIG_MIDI_ETH_IP_DHCP) for router connection
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_eth_init(void);

/**
 * @brief Get the Ethernet network interface handle
 * 
 * @return esp_netif_t* pointer to Ethernet netif or NULL if not initialized
 */
esp_netif_t* midi_eth_get_netif(void);


#endif /* MIDI_ETHERNET_H */
