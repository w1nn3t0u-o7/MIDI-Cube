/**
 * @file midi_router.h
 * @brief MIDI Router - Central Message Routing
 * 
 * Implements flexible, configurable routing between 4 transports:
 * - UART/DIN (MIDI 1.0)
 * - USB (MIDI 1.0 / 2.0)
 * - Ethernet (MIDI 2.0 over UDP)
 * - WiFi (MIDI 2.0 over UDP)
 * 
 * Features:
 * - 4×4 routing matrix (any input → any outputs)
 * - Automatic protocol translation (MIDI 1.0 ↔ UMP)
 * - Message filtering (channel, type, etc.)
 * - Real-time performance (<1ms latency)
 * - Configuration save/load (NVS)
 * - Activity monitoring and statistics
 */

#ifndef MIDI_ROUTER_H
#define MIDI_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "midi_types.h"
#include "ump_types.h"

/**
 * @brief Transport identifiers
 */
typedef enum {
    MIDI_TRANSPORT_UART,  /**< UART/DIN-5 (MIDI 1.0) */
    MIDI_TRANSPORT_USB,       /**< USB (MIDI 1.0/2.0) */
    MIDI_TRANSPORT_ETHERNET,  /**< Ethernet (MIDI 2.0) */
    MIDI_TRANSPORT_WIFI,      /**< WiFi (MIDI 2.0) */
    MIDI_TRANSPORT_COUNT      /**< Number of transports */
} midi_transport_t;

typedef enum {
    MIDI_FORMAT_1_0,  /**< MIDI 1.0 format */
    MIDI_FORMAT_2_0   /**< MIDI 2.0 format */
} midi_format_t;

/**
 * @brief MIDI packet format (unified internal format)
 */
typedef struct {
    midi_transport_t source;     /**< Source transport */
    midi_transport_t destination; /**< Destination (0xFF = broadcast) */
    uint8_t format;               /**< 0=MIDI1.0, 1=UMP */
    
    union {
        midi_message_t midi1;     /**< MIDI 1.0 message */
        ump_packet_t ump;         /**< UMP packet */
    } data;
} midi_router_packet_t;

/**
 * @brief Message filter configuration
 */
typedef struct {
    bool enabled;                 /**< Enable filtering */
    uint16_t channel_mask;        /**< Channel filter (bit per channel) */
    uint8_t msg_type_mask;        /**< Message type filter */
    bool block_active_sensing;    /**< Block 0xFE messages */
    bool block_clock;             /**< Block 0xF8 messages */
} midi_filter_t;

/**
 * @brief Router configuration
 */
typedef struct {
    // Routing matrix [source][destination]
    bool routing_matrix[MIDI_TRANSPORT_COUNT][MIDI_TRANSPORT_COUNT];
    
    // Filters per input
    midi_filter_t input_filters[MIDI_TRANSPORT_COUNT];
    
    // Global settings
    bool auto_translate;          /**< Auto MIDI 1.0 ↔ UMP translation */
    bool merge_inputs;            /**< Merge all inputs to all outputs */
    uint8_t default_group;        /**< Default UMP group (0-15) */
} midi_router_config_t;

/**
 * @brief Router statistics
 */
typedef struct {
    uint32_t packets_routed[MIDI_TRANSPORT_COUNT][MIDI_TRANSPORT_COUNT];
    uint32_t packets_dropped[MIDI_TRANSPORT_COUNT];
    uint32_t packets_filtered[MIDI_TRANSPORT_COUNT];
    uint32_t translations_1to2;
    uint32_t translations_2to1;
    uint32_t routing_errors;
} midi_router_stats_t;

void uart_rx_callback(const midi_message_t *msg, void *ctx);

/**
 * @brief Initialize MIDI router
 * 
 * Creates router task, initializes buffers, loads configuration from NVS
 * 
 * @param config Initial router configuration (NULL = load from NVS)
 * @return ESP_OK on success
 */
esp_err_t midi_router_init(const midi_router_config_t *config);

/**
 * @brief Deinitialize MIDI router
 * 
 * Stops router task, saves configuration to NVS, frees resources
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_router_deinit(void);

/**
 * @brief Send packet to router (from transport layer)
 * 
 * Called by transport RX callbacks to inject packet into router
 * 
 * @param packet MIDI packet to route
 * @return ESP_OK on success, ESP_ERR_NO_MEM if buffer full
 */
esp_err_t midi_router_send(const midi_router_packet_t *packet);

/**
 * @brief Set routing matrix entry
 * 
 * Enable/disable routing from source to destination
 * 
 * @param source Source transport
 * @param destination Destination transport
 * @param enable true = route, false = block
 * @return ESP_OK on success
 */
esp_err_t midi_router_set_route(midi_transport_t source,
                                 midi_transport_t destination,
                                 bool enable);

/**
 * @brief Get routing matrix entry
 * 
 * @param source Source transport
 * @param destination Destination transport
 * @param enabled Output: current routing state
 * @return ESP_OK on success
 */
esp_err_t midi_router_get_route(midi_transport_t source,
                                 midi_transport_t destination,
                                 bool *enabled);

/**
 * @brief Set input filter for transport
 * 
 * @param transport Transport to filter
 * @param filter Filter configuration
 * @return ESP_OK on success
 */
esp_err_t midi_router_set_filter(midi_transport_t transport,
                                  const midi_filter_t *filter);

/**
 * @brief Enable/disable merge mode
 * 
 * In merge mode, all inputs are routed to all outputs
 * (overrides routing matrix)
 * 
 * @param enable true = merge all, false = use matrix
 * @return ESP_OK on success
 */
esp_err_t midi_router_set_merge_mode(bool enable);

/**
 * @brief Get router statistics
 * 
 * @param stats Output: statistics structure
 * @return ESP_OK on success
 */
esp_err_t midi_router_get_stats(midi_router_stats_t *stats);

/**
 * @brief Reset statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_router_reset_stats(void);

/**
 * @brief Save configuration to NVS
 * 
 * Persists routing matrix and filters
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_router_save_config(void);

/**
 * @brief Load configuration from NVS
 * 
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no saved config
 */
esp_err_t midi_router_load_config(void);

/**
 * @brief Reset configuration to defaults
 * 
 * Default: All routes enabled, no filtering
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_router_reset_config(void);

/**
 * @brief Get transport name string
 * 
 * @param transport Transport ID
 * @return Human-readable name (e.g., "UART")
 */
const char* midi_router_get_transport_name(midi_transport_t transport);

#endif /* MIDI_ROUTER_H */
