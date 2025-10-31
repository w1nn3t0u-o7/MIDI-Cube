/**
 * @file midi_wifi_session.h
 * @brief MIDI WiFi Session Management - Internal API
 * 
 * Handles session establishment, keepalive, and tear-down
 * per Network MIDI 2.0 spec
 */

#ifndef MIDI_WIFI_SESSION_H
#define MIDI_WIFI_SESSION_H

#include "midi_wifi.h"

/**
 * @brief Session packet types (Network MIDI 2.0)[file:4]
 */
typedef enum {
    MIDI_WIFI_PKT_UMP = 0x00,           /**< UMP payload */
    MIDI_WIFI_PKT_SESSION_START = 0x01,  /**< Session start request */
    MIDI_WIFI_PKT_SESSION_ACK = 0x02,    /**< Session start acknowledgment */
    MIDI_WIFI_PKT_SESSION_END = 0x03,    /**< Session end notification */
    MIDI_WIFI_PKT_KEEPALIVE = 0x04,      /**< Keepalive heartbeat */
    MIDI_WIFI_PKT_RETRANSMIT_REQ = 0x05, /**< Retransmit request */
} midi_wifi_packet_type_t;

/**
 * @brief Initialize session manager
 * 
 * @param config WiFi MIDI configuration
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_session_init(const midi_wifi_config_t *config);

/**
 * @brief Deinitialize session manager
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_session_deinit(void);

/**
 * @brief Handle incoming packet
 * 
 * Processes session control packets and UMP payload
 * 
 * @param data Packet data
 * @param len Packet length
 * @param src_ip Source IP address
 * @param src_port Source UDP port
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_session_handle_packet(const uint8_t *data, size_t len,
                                           const char *src_ip, uint16_t src_port);

/**
 * @brief Send session keepalive
 * 
 * Sends keepalive to all active sessions
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_wifi_session_send_keepalive(void);

#endif /* MIDI_WIFI_SESSION_H */
