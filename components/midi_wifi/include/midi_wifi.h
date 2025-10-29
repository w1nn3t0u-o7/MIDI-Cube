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

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "ump_types.h"
#include "esp_netif.h"

/**
 * @brief WiFi MIDI operating mode
 */
typedef enum {
    MIDI_WIFI_MODE_HOST = 0,    /**< Host mode (discoverable, accepts connections) */
    MIDI_WIFI_MODE_CLIENT,       /**< Client mode (connects to discovered hosts) */
    MIDI_WIFI_MODE_BOTH          /**< Both host and client simultaneously */
} midi_wifi_mode_t;

/**
 * @brief Session state
 */
typedef enum {
    MIDI_WIFI_SESSION_DISCONNECTED = 0,
    MIDI_WIFI_SESSION_CONNECTING,
    MIDI_WIFI_SESSION_CONNECTED,
    MIDI_WIFI_SESSION_ERROR
} midi_wifi_session_state_t;

/**
 * @brief Remote peer information
 */
typedef struct {
    char ip_addr[16];            /**< IPv4 address string */
    uint16_t port;               /**< UDP port */
    char endpoint_name[64];      /**< UMP Endpoint name */
    uint8_t session_id;          /**< Session identifier */
    midi_wifi_session_state_t state;
    uint32_t last_rx_time_ms;    /**< Last packet received timestamp */
    uint32_t packets_rx;         /**< Packets received */
    uint32_t packets_tx;         /**< Packets transmitted */
    uint32_t packets_lost;       /**< Packets lost (detected) */
} midi_wifi_peer_t;

/**
 * @brief Discovered MIDI device info
 */
typedef struct {
    char ip_addr[16];            /**< IPv4 address */
    uint16_t port;               /**< UDP port */
    char endpoint_name[64];      /**< Endpoint name */
    char instance_name[64];      /**< mDNS instance name */
    bool supports_fec;           /**< Forward Error Correction support */
    bool supports_retransmit;    /**< Retransmit support */
} midi_wifi_discovered_device_t;

/**
 * @brief UMP receive callback
 * 
 * Called when UMP packet received from network
 * 
 * @param ump Received UMP packet
 * @param peer Source peer info
 * @param user_ctx User context
 */
typedef void (*midi_wifi_rx_callback_t)(const ump_packet_t *ump, 
                                         const midi_wifi_peer_t *peer,
                                         void *user_ctx);

/**
 * @brief Connection status callback
 * 
 * Called when peer connects or disconnects
 * 
 * @param peer Peer information
 * @param connected true if connected, false if disconnected
 * @param user_ctx User context
 */
typedef void (*midi_wifi_conn_callback_t)(const midi_wifi_peer_t *peer,
                                           bool connected,
                                           void *user_ctx);

/**
 * @brief Device discovery callback
 * 
 * Called when new MIDI device discovered via mDNS
 * 
 * @param device Discovered device info
 * @param user_ctx User context
 */
typedef void (*midi_wifi_discovery_callback_t)(const midi_wifi_discovered_device_t *device,
                                                void *user_ctx);

/**
 * @brief WiFi MIDI configuration
 */
typedef struct {
    midi_wifi_mode_t mode;           /**< Operating mode */
    uint16_t host_port;              /**< Host UDP port (5004 default) */
    char endpoint_name[64];          /**< UMP Endpoint name */
    uint8_t max_clients;             /**< Max simultaneous clients (host mode) */
    
    bool enable_fec;                 /**< Enable Forward Error Correction */
    bool enable_retransmit;          /**< Enable retransmit support */
    uint16_t retransmit_buffer_size; /**< Retransmit buffer (packets) */
    
    bool enable_mdns;                /**< Enable mDNS discovery */
    
    midi_wifi_rx_callback_t rx_callback;
    midi_wifi_conn_callback_t conn_callback;
    midi_wifi_discovery_callback_t discovery_callback;
    void *callback_ctx;
} midi_wifi_config_t;

/**
 * @brief WiFi MIDI statistics
 */
typedef struct {
    uint32_t packets_rx_total;
    uint32_t packets_tx_total;
    uint32_t packets_lost_total;
    uint32_t packets_recovered_fec;
    uint32_t packets_retransmitted;
    uint32_t active_sessions;
    uint32_t discovery_count;
} midi_wifi_stats_t;

/**
 * @brief Initialize WiFi MIDI driver
 * 
 * Initializes WiFi, starts UDP socket, begins mDNS discovery
 * 
 * @param config WiFi MIDI configuration
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_init(const midi_wifi_config_t *config);

/**
 * @brief Deinitialize WiFi MIDI driver
 * 
 * Closes all connections, stops services
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_deinit(void);

/**
 * @brief Connect WiFi to access point
 * 
 * Must be called before MIDI communication
 * 
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @param timeout_ms Connection timeout (0 = blocking)
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);

/**
 * @brief Disconnect from WiFi
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_disconnect(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected to AP
 */
bool midi_wifi_is_connected(void);

/**
 * @brief Send UMP to all connected peers
 * 
 * Broadcasts UMP to all active sessions
 * 
 * @param ump UMP packet to send
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_send_ump(const ump_packet_t *ump);

/**
 * @brief Send UMP to specific peer
 * 
 * @param ump UMP packet to send
 * @param peer_ip Target peer IP address
 * @param peer_port Target peer UDP port
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_send_ump_to(const ump_packet_t *ump, 
                                 const char *peer_ip, 
                                 uint16_t peer_port);

/**
 * @brief Connect to discovered device (client mode)
 * 
 * Establishes session with remote host
 * 
 * @param ip_addr Host IP address
 * @param port Host UDP port
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_connect_to_peer(const char *ip_addr, uint16_t port);

/**
 * @brief Disconnect from peer
 * 
 * Closes session with remote peer
 * 
 * @param ip_addr Peer IP address
 * @param port Peer UDP port
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_disconnect_peer(const char *ip_addr, uint16_t port);

/**
 * @brief Get list of active peers
 * 
 * @param peers Output array of peer info
 * @param max_peers Size of peers array
 * @param num_peers Output: actual number of peers
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_get_peers(midi_wifi_peer_t *peers, 
                               uint8_t max_peers,
                               uint8_t *num_peers);

/**
 * @brief Get list of discovered devices
 * 
 * Returns devices found via mDNS
 * 
 * @param devices Output array of discovered devices
 * @param max_devices Size of devices array
 * @param num_devices Output: actual number of devices
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_get_discovered_devices(midi_wifi_discovered_device_t *devices,
                                            uint8_t max_devices,
                                            uint8_t *num_devices);

/**
 * @brief Start mDNS discovery scan
 * 
 * Triggers active scan for MIDI devices on network
 * 
 * @param scan_duration_ms Scan duration (0 = continuous)
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_start_discovery(uint32_t scan_duration_ms);

/**
 * @brief Stop mDNS discovery scan
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_stop_discovery(void);

/**
 * @brief Get WiFi MIDI statistics
 * 
 * @param stats Output statistics structure
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_get_stats(midi_wifi_stats_t *stats);

/**
 * @brief Reset statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_reset_stats(void);

/**
 * @brief Get local IP address
 * 
 * @param ip_str Output buffer (min 16 bytes)
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_get_local_ip(char *ip_str);

/**
 * @brief Set endpoint name
 * 
 * Updates the UMP Endpoint name advertised via mDNS
 * 
 * @param name New endpoint name (max 63 chars)
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_set_endpoint_name(const char *name);

/**
 * @brief Enable/disable Forward Error Correction
 * 
 * @param enable true to enable FEC
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_set_fec_enabled(bool enable);

/**
 * @brief Request retransmission of lost packet
 * 
 * Sends retransmit request to peer (if supported)
 * 
 * @param peer_ip Peer IP address
 * @param peer_port Peer UDP port
 * @param sequence_number Lost packet sequence number
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_request_retransmit(const char *peer_ip,
                                        uint16_t peer_port,
                                        uint32_t sequence_number);

#endif /* MIDI_WIFI_H */

