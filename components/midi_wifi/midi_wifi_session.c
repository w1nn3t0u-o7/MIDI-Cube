/**
 * @file midi_wifi_session.c
 * @brief MIDI WiFi Session Management Implementation
 * 
 * Handles session lifecycle per Network MIDI 2.0 spec
 */

#include "midi_wifi_session.h"
#include "midi_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "wifi_session";

// External access to main state (declared in midi_wifi.c)
extern midi_wifi_state_t g_wifi_state;

/**
 * @brief Session packet header structure[file:4]
 */
typedef struct __attribute__((packed)) {
    uint8_t packet_type;        // MIDI_WIFI_PKT_*
    uint32_t sequence_number;   // Packet sequence
    uint8_t session_id;         // Session identifier (optional)
} session_packet_header_t;

/**
 * @brief Find peer by IP and port
 */
static midi_wifi_peer_t* find_peer(const char *ip_addr, uint16_t port) {
    for (int i = 0; i < g_wifi_state.num_active_peers; i++) {
        midi_wifi_peer_t *peer = &g_wifi_state.peers[i];
        if (strcmp(peer->ip_addr, ip_addr) == 0 && peer->port == port) {
            return peer;
        }
    }
    return NULL;
}

/**
 * @brief Add new peer to list
 */
static midi_wifi_peer_t* add_peer(const char *ip_addr, uint16_t port) {
    if (g_wifi_state.num_active_peers >= CONFIG_MIDI_WIFI_MAX_CLIENTS) {
        ESP_LOGW(TAG, "Max peers reached, cannot add %s:%d", ip_addr, port);
        return NULL;
    }
    
    midi_wifi_peer_t *peer = &g_wifi_state.peers[g_wifi_state.num_active_peers++];
    memset(peer, 0, sizeof(midi_wifi_peer_t));
    
    strncpy(peer->ip_addr, ip_addr, sizeof(peer->ip_addr) - 1);
    peer->port = port;
    peer->session_id = g_wifi_state.num_active_peers;  // Simple ID assignment
    peer->state = MIDI_WIFI_SESSION_CONNECTING;
    peer->last_rx_time_ms = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "Added peer %s:%d (session %d)", ip_addr, port, peer->session_id);
    
    return peer;
}

/**
 * @brief Remove peer from list
 */
static void remove_peer(midi_wifi_peer_t *peer) {
    if (!peer) return;
    
    ESP_LOGI(TAG, "Removing peer %s:%d", peer->ip_addr, peer->port);
    
    // Shift remaining peers
    int peer_idx = peer - g_wifi_state.peers;
    if (peer_idx < g_wifi_state.num_active_peers - 1) {
        memmove(peer, peer + 1, 
                (g_wifi_state.num_active_peers - peer_idx - 1) * sizeof(midi_wifi_peer_t));
    }
    
    g_wifi_state.num_active_peers--;
}

/**
 * @brief Send session start acknowledgment[file:4]
 */
static esp_err_t send_session_ack(const char *ip_addr, uint16_t port, uint8_t session_id) {
    uint8_t packet[6];
    packet[0] = MIDI_WIFI_PKT_SESSION_ACK;
    memcpy(&packet[1], &g_wifi_state.tx_sequence_num, 4);
    packet[5] = session_id;
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_addr, &dest_addr.sin_addr);
    
    int sent = sendto(g_wifi_state.sock_fd, packet, sizeof(packet), 0,
                     (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    
    if (sent != sizeof(packet)) {
        ESP_LOGW(TAG, "Failed to send session ACK to %s:%d", ip_addr, port);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sent SESSION_ACK to %s:%d", ip_addr, port);
    return ESP_OK;
}

/**
 * @brief Send keepalive packet[file:4]
 */
static esp_err_t send_keepalive(midi_wifi_peer_t *peer) {
    uint8_t packet[5];
    packet[0] = MIDI_WIFI_PKT_KEEPALIVE;
    memcpy(&packet[1], &g_wifi_state.tx_sequence_num, 4);
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(peer->port);
    inet_pton(AF_INET, peer->ip_addr, &dest_addr.sin_addr);
    
    int sent = sendto(g_wifi_state.sock_fd, packet, sizeof(packet), 0,
                     (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    
    if (sent != sizeof(packet)) {
        return ESP_FAIL;
    }
    
    ESP_LOGV(TAG, "Sent KEEPALIVE to %s:%d", peer->ip_addr, peer->port);
    return ESP_OK;
}

/**
 * @brief Handle session start request[file:4]
 */
static esp_err_t handle_session_start(const uint8_t *data, size_t len,
                                       const char *src_ip, uint16_t src_port) {
    ESP_LOGI(TAG, "SESSION_START from %s:%d", src_ip, src_port);
    
    xSemaphoreTake(g_wifi_state.peers_mutex, portMAX_DELAY);
    
    // Check if peer already exists
    midi_wifi_peer_t *peer = find_peer(src_ip, src_port);
    if (!peer) {
        peer = add_peer(src_ip, src_port);
        if (!peer) {
            xSemaphoreGive(g_wifi_state.peers_mutex);
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Mark as connected
    peer->state = MIDI_WIFI_SESSION_CONNECTED;
    peer->last_rx_time_ms = esp_timer_get_time() / 1000;
    
    xSemaphoreGive(g_wifi_state.peers_mutex);
    
    // Send acknowledgment
    send_session_ack(src_ip, src_port, peer->session_id);
    
    // Notify application
    if (g_wifi_state.config.conn_callback) {
        g_wifi_state.config.conn_callback(peer, true, g_wifi_state.config.callback_ctx);
    }
    
    return ESP_OK;
}

/**
 * @brief Handle session end notification[file:4]
 */
static esp_err_t handle_session_end(const uint8_t *data, size_t len,
                                     const char *src_ip, uint16_t src_port) {
    ESP_LOGI(TAG, "SESSION_END from %s:%d", src_ip, src_port);
    
    xSemaphoreTake(g_wifi_state.peers_mutex, portMAX_DELAY);
    
    midi_wifi_peer_t *peer = find_peer(src_ip, src_port);
    if (peer) {
        // Notify application before removing
        if (g_wifi_state.config.conn_callback) {
            g_wifi_state.config.conn_callback(peer, false, g_wifi_state.config.callback_ctx);
        }
        
        remove_peer(peer);
    }
    
    xSemaphoreGive(g_wifi_state.peers_mutex);
    
    return ESP_OK;
}

/**
 * @brief Handle keepalive packet[file:4]
 */
static esp_err_t handle_keepalive(const uint8_t *data, size_t len,
                                   const char *src_ip, uint16_t src_port) {
    xSemaphoreTake(g_wifi_state.peers_mutex, portMAX_DELAY);
    
    midi_wifi_peer_t *peer = find_peer(src_ip, src_port);
    if (peer) {
        peer->last_rx_time_ms = esp_timer_get_time() / 1000;
        ESP_LOGV(TAG, "KEEPALIVE from %s:%d", src_ip, src_port);
    }
    
    xSemaphoreGive(g_wifi_state.peers_mutex);
    
    return ESP_OK;
}

/**
 * @brief Handle UMP payload packet[file:4]
 */
static esp_err_t handle_ump_payload(const uint8_t *data, size_t len,
                                     const char *src_ip, uint16_t src_port) {
    if (len < 5) {  // Header (1 + 4) + at least 4 bytes UMP
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Extract sequence number
    uint32_t sequence;
    memcpy(&sequence, &data[1], 4);
    
    // Parse UMP packets (starting at offset 5)
    const uint8_t *ump_data = &data[5];
    size_t ump_len = len - 5;
    
    xSemaphoreTake(g_wifi_state.peers_mutex, portMAX_DELAY);
    
    midi_wifi_peer_t *peer = find_peer(src_ip, src_port);
    if (!peer || peer->state != MIDI_WIFI_SESSION_CONNECTED) {
        xSemaphoreGive(g_wifi_state.peers_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    peer->last_rx_time_ms = esp_timer_get_time() / 1000;
    peer->packets_rx++;
    
    xSemaphoreGive(g_wifi_state.peers_mutex);
    
    // Parse UMP words
    size_t offset = 0;
    while (offset + 4 <= ump_len) {
        ump_packet_t ump;
        memset(&ump, 0, sizeof(ump));
        
        // Read first word to determine packet size
        memcpy(&ump.words[0], &ump_data[offset], 4);
        uint8_t mt = (ump.words[0] >> 28) & 0x0F;
        
        // Determine number of words
        if (mt <= 0x2) ump.num_words = 1;       // 32-bit
        else if (mt <= 0x5) ump.num_words = 2;  // 64-bit
        else if (mt <= 0xC) ump.num_words = 3;  // 96-bit
        else ump.num_words = 4;                  // 128-bit
        
        // Check if enough data
        if (offset + (ump.num_words * 4) > ump_len) {
            ESP_LOGW(TAG, "Incomplete UMP packet");
            break;
        }
        
        // Read remaining words
        for (int i = 1; i < ump.num_words; i++) {
            memcpy(&ump.words[i], &ump_data[offset + (i * 4)], 4);
        }
        
        ump.message_type = mt;
        ump.group = (ump.words[0] >> 24) & 0x0F;
        
        // Call user callback
        if (g_wifi_state.config.rx_callback) {
            g_wifi_state.config.rx_callback(&ump, peer, g_wifi_state.config.callback_ctx);
        }
        
        offset += ump.num_words * 4;
    }
    
    return ESP_OK;
}

/**
 * @brief Handle incoming packet
 */
esp_err_t midi_wifi_session_handle_packet(const uint8_t *data, size_t len,
                                           const char *src_ip, uint16_t src_port) {
    if (len < 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    uint8_t packet_type = data[0];
    
    switch (packet_type) {
        case MIDI_WIFI_PKT_SESSION_START:
            return handle_session_start(data, len, src_ip, src_port);
            
        case MIDI_WIFI_PKT_SESSION_END:
            return handle_session_end(data, len, src_ip, src_port);
            
        case MIDI_WIFI_PKT_KEEPALIVE:
            return handle_keepalive(data, len, src_ip, src_port);
            
        case MIDI_WIFI_PKT_UMP:
            return handle_ump_payload(data, len, src_ip, src_port);
            
        case MIDI_WIFI_PKT_RETRANSMIT_REQ:
            // TODO: Handle retransmit request
            ESP_LOGD(TAG, "Retransmit request from %s:%d", src_ip, src_port);
            return ESP_OK;
            
        default:
            ESP_LOGW(TAG, "Unknown packet type: 0x%02X from %s:%d", 
                     packet_type, src_ip, src_port);
            return ESP_ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Send keepalive to all peers
 */
esp_err_t midi_wifi_session_send_keepalive(void) {
    xSemaphoreTake(g_wifi_state.peers_mutex, portMAX_DELAY);
    
    uint32_t current_time_ms = esp_timer_get_time() / 1000;
    
    for (int i = 0; i < g_wifi_state.num_active_peers; i++) {
        midi_wifi_peer_t *peer = &g_wifi_state.peers[i];
        
        if (peer->state != MIDI_WIFI_SESSION_CONNECTED) {
            continue;
        }
        
        // Check for timeout
        if (current_time_ms - peer->last_rx_time_ms > MIDI_WIFI_SESSION_TIMEOUT) {
            ESP_LOGW(TAG, "Peer %s:%d timed out", peer->ip_addr, peer->port);
            
            // Notify application
            if (g_wifi_state.config.conn_callback) {
                g_wifi_state.config.conn_callback(peer, false, g_wifi_state.config.callback_ctx);
            }
            
            remove_peer(peer);
            i--;  // Adjust index after removal
            continue;
        }
        
        // Send keepalive
        send_keepalive(peer);
    }
    
    xSemaphoreGive(g_wifi_state.peers_mutex);
    
    return ESP_OK;
}

/**
 * @brief Initialize session manager
 */
esp_err_t midi_wifi_session_init(const midi_wifi_config_t *config) {
    ESP_LOGI(TAG, "Session manager initialized");
    return ESP_OK;
}

/**
 * @brief Deinitialize session manager
 */
esp_err_t midi_wifi_session_deinit(void) {
    // Send SESSION_END to all peers
    xSemaphoreTake(g_wifi_state.peers_mutex, portMAX_DELAY);
    
    for (int i = 0; i < g_wifi_state.num_active_peers; i++) {
        midi_wifi_peer_t *peer = &g_wifi_state.peers[i];
        
        uint8_t packet[5];
        packet[0] = MIDI_WIFI_PKT_SESSION_END;
        memcpy(&packet[1], &g_wifi_state.tx_sequence_num, 4);
        
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(peer->port);
        inet_pton(AF_INET, peer->ip_addr, &dest_addr.sin_addr);
        
        sendto(g_wifi_state.sock_fd, packet, sizeof(packet), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        ESP_LOGI(TAG, "Sent SESSION_END to %s:%d", peer->ip_addr, peer->port);
    }
    
    g_wifi_state.num_active_peers = 0;
    
    xSemaphoreGive(g_wifi_state.peers_mutex);
    
    return ESP_OK;
}
