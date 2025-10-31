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

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "ump_types.h"
#include "esp_eth.h"

// Reuse WiFi types for consistency (same Network MIDI 2.0 protocol)
typedef struct midi_wifi_peer midi_ethernet_peer_t;
typedef struct midi_wifi_discovered_device midi_ethernet_discovered_device_t;
typedef enum midi_wifi_session_state midi_ethernet_session_state_t;

/**
 * @brief UMP receive callback
 */
typedef void (*midi_ethernet_rx_callback_t)(const ump_packet_t *ump,
                                             const midi_ethernet_peer_t *peer,
                                             void *user_ctx);

/**
 * @brief Connection status callback
 */
typedef void (*midi_ethernet_conn_callback_t)(const midi_ethernet_peer_t *peer,
                                               bool connected,
                                               void *user_ctx);

/**
 * @brief Device discovery callback
 */
typedef void (*midi_ethernet_discovery_callback_t)(const midi_ethernet_discovered_device_t *device,
                                                    void *user_ctx);

/**
 * @brief Ethernet MIDI configuration
 */
typedef struct {
    // SPI configuration (W5500)
    int spi_host;               /**< SPI host (SPI2_HOST or SPI3_HOST) */
    int spi_clock_speed_mhz;    /**< SPI clock (up to 80 MHz) */
    int gpio_sclk;              /**< GPIO for SPI CLK */
    int gpio_mosi;              /**< GPIO for SPI MOSI */
    int gpio_miso;              /**< GPIO for SPI MISO */
    int gpio_cs;                /**< GPIO for W5500 CS */
    int gpio_int;               /**< GPIO for W5500 INT (-1 = polling) */
    
    // Network configuration
    bool use_dhcp;              /**< Use DHCP (true) or static IP (false) */
    char static_ip[16];         /**< Static IP (if DHCP disabled) */
    char netmask[16];           /**< Netmask (if DHCP disabled) */
    char gateway[16];           /**< Gateway (if DHCP disabled) */
    
    // MIDI configuration
    uint16_t host_port;         /**< UDP port (5004 default) */
    char endpoint_name[64];     /**< UMP Endpoint name */
    uint8_t max_clients;        /**< Max simultaneous clients */
    
    bool enable_fec;            /**< Forward Error Correction */
    bool enable_retransmit;     /**< Retransmit support */
    uint16_t retransmit_buffer_size;
    
    bool enable_mdns;           /**< mDNS discovery */
    
    // Callbacks
    midi_ethernet_rx_callback_t rx_callback;
    midi_ethernet_conn_callback_t conn_callback;
    midi_ethernet_discovery_callback_t discovery_callback;
    void *callback_ctx;
} midi_ethernet_config_t;

/**
 * @brief Ethernet MIDI statistics
 */
typedef struct {
    uint32_t packets_rx_total;
    uint32_t packets_tx_total;
    uint32_t packets_lost_total;
    uint32_t packets_recovered_fec;
    uint32_t active_sessions;
    bool link_up;
    bool ip_assigned;
} midi_ethernet_stats_t;

/**
 * @brief Initialize Ethernet MIDI driver
 * 
 * Initializes W5500 via SPI, starts UDP socket, begins mDNS
 * 
 * @param config Ethernet MIDI configuration
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_init(const midi_ethernet_config_t *config);

/**
 * @brief Deinitialize Ethernet MIDI driver
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_deinit(void);

/**
 * @brief Wait for Ethernet link up
 * 
 * Blocks until W5500 detects physical link
 * 
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return ESP_OK if link up, ESP_ERR_TIMEOUT if timeout
 */
esp_err_t midi_ethernet_wait_for_link(uint32_t timeout_ms);

/**
 * @brief Check if Ethernet link is up
 * 
 * @return true if cable connected and link established
 */
bool midi_ethernet_is_link_up(void);

/**
 * @brief Send UMP to all connected peers
 * 
 * @param ump UMP packet to send
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_send_ump(const ump_packet_t *ump);

/**
 * @brief Send UMP to specific peer
 * 
 * @param ump UMP packet
 * @param peer_ip Target IP address
 * @param peer_port Target UDP port
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_send_ump_to(const ump_packet_t *ump,
                                     const char *peer_ip,
                                     uint16_t peer_port);

/**
 * @brief Get Ethernet statistics
 * 
 * @param stats Output statistics structure
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_get_stats(midi_ethernet_stats_t *stats);

/**
 * @brief Get local IP address
 * 
 * @param ip_str Output buffer (min 16 bytes)
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_get_local_ip(char *ip_str);

/**
 * @brief Get MAC address
 * 
 * @param mac Output buffer (6 bytes)
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_get_mac(uint8_t *mac);

/**
 * @brief Start mDNS discovery scan
 * 
 * @param scan_duration_ms Scan duration
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_start_discovery(uint32_t scan_duration_ms);

/**
 * @brief Get list of active peers
 * 
 * @param peers Output array
 * @param max_peers Array size
 * @param num_peers Output: actual count
 * @return ESP_OK on success
 */
esp_err_t midi_ethernet_get_peers(midi_ethernet_peer_t *peers,
                                   uint8_t max_peers,
                                   uint8_t *num_peers);

#endif /* MIDI_ETHERNET_H */
