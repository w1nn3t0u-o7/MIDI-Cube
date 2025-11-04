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

typedef struct {
    esp_netif_t *eth_netif;
    esp_eth_handle_t eth_handle;
    eth_connected_cb_t connected_cb;
    eth_disconnected_cb_t disconnected_cb;
    bool is_connected;
    bool link_up;

    spi_host_device_t spi_host;  // SPI2_HOST or SPI3_HOST
    int spi_clock_mhz;            // SPI clock speed (8-36 MHz recommended)
    int gpio_mosi;
    int gpio_miso;
    int gpio_sclk;
    int gpio_cs;
    int gpio_int;                 // Interrupt GPIO
    int gpio_rst;                 // Reset GPIO (-1 if not used)
} midi_eth_config_t;

/**
 * @brief Initialize W5500 Ethernet
 * 
 * @param config Ethernet/W5500 configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t midi_eth_init(void);

/**
 * @brief Register connection callbacks
 * 
 * @param connected_cb Called when Ethernet connects and gets IP
 * @param disconnected_cb Called when Ethernet link goes down
 */
void midi_eth_register_callbacks(eth_connected_cb_t connected_cb,
                                          eth_disconnected_cb_t disconnected_cb);

/**
 * @brief Get Ethernet network interface
 * 
 * @return esp_netif_t* Network interface or NULL if not initialized
 */
esp_netif_t* midi_eth_get_netif(void);

/**
 * @brief Check if Ethernet is connected
 * 
 * @return true if link is up and has valid IP
 */
bool midi_eth_is_connected(void);

/**
 * @brief Stop and deinitialize Ethernet
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t midi_eth_deinit(void);

#endif /* MIDI_ETHERNET_H */
