/**
 * @file midi_usb.h
 * @brief USB MIDI Transport - Dual Mode (Host & Device)
 * 
 * Supports:
 * - USB Device Mode (when connected to PC)
 * - USB Host Mode (when MIDI device connects)
 * - MIDI 1.0 (32-bit USB-MIDI Event Packets)
 * - MIDI 2.0 (Universal MIDI Packets - UMP)
 * - Automatic mode detection via USB ID pin
 * 
 * Based on:
 * - USB Device Class Definition for MIDI Devices v1.0
 * - USB Device Class Definition for MIDI Devices v2.0
 * - ESP32-S3 USB OTG Controller
 */

#ifndef MIDI_USB_H
#define MIDI_USB_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "midi_types.h"
#include "ump_types.h"

/**
 * @brief USB operating mode
 */
typedef enum {
    MIDI_USB_MODE_NONE = 0,      /**< Not initialized */
    MIDI_USB_MODE_DEVICE,         /**< USB Device (connect to PC) */
    MIDI_USB_MODE_HOST,           /**< USB Host (connect MIDI device) */
} midi_usb_mode_t;

/**
 * @brief USB MIDI protocol version
 */
typedef enum {
    MIDI_USB_PROTOCOL_1_0 = 0,    /**< USB MIDI 1.0 (32-bit packets) */
    MIDI_USB_PROTOCOL_2_0,        /**< USB MIDI 2.0 (UMP) */
} midi_usb_protocol_t;

/**
 * @brief USB MIDI packet (unified format)
 * 
 * Can contain either:
 * - USB-MIDI 1.0 Event Packet (4 bytes)
 * - UMP (Universal MIDI Packet) (4-16 bytes)
 */
typedef struct {
    uint8_t cable_number;         /**< Virtual cable (0-15) */
    midi_usb_protocol_t protocol; /**< MIDI 1.0 or 2.0 */
    uint32_t timestamp_us;        /**< Reception timestamp */
    
    union {
        // USB-MIDI 1.0 (4 bytes)
        struct {
            uint8_t cin;          /**< Code Index Number */
            uint8_t midi_bytes[3]; /**< MIDI bytes */
        } midi1;
        
        // UMP (Universal MIDI Packet)
        ump_packet_t ump;
    } data;
} midi_usb_packet_t;

/**
 * @brief USB MIDI RX callback
 * 
 * Called when packet received from USB
 * 
 * @param packet Received USB MIDI packet
 * @param user_ctx User context
 */
typedef void (*midi_usb_rx_callback_t)(const midi_usb_packet_t *packet, void *user_ctx);

/**
 * @brief USB connection callback
 * 
 * Called when USB connection status changes
 * 
 * @param connected true if device connected
 * @param user_ctx User context
 */
typedef void (*midi_usb_conn_callback_t)(bool connected, void *user_ctx);

/**
 * @brief USB MIDI configuration
 */
typedef struct {
    midi_usb_mode_t mode;              /**< USB mode (auto/device/host) */
    bool enable_midi2;                 /**< Enable MIDI 2.0 support */
    uint8_t num_cables;                /**< Number of virtual cables (1-16) */
    
    midi_usb_rx_callback_t rx_callback;     /**< RX packet callback */
    midi_usb_conn_callback_t conn_callback; /**< Connection callback */
    void *callback_ctx;                     /**< User context for callbacks */
} midi_usb_config_t;

/**
 * @brief USB MIDI statistics
 */
typedef struct {
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t packets_dropped_rx;
    uint32_t packets_dropped_tx;
    uint32_t usb_errors;
    midi_usb_mode_t current_mode;
    midi_usb_protocol_t current_protocol;
    bool connected;
} midi_usb_stats_t;

/**
 * @brief Initialize USB MIDI driver
 * 
 * Configures USB OTG, detects mode, starts appropriate stack
 * 
 * @param config USB MIDI configuration
 * @return ESP_OK on success
 */
esp_err_t midi_usb_init(const midi_usb_config_t *config);

/**
 * @brief Deinitialize USB MIDI driver
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_usb_deinit(void);

/**
 * @brief Send USB MIDI packet
 * 
 * @param packet Packet to send
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if buffer full
 */
esp_err_t midi_usb_send_packet(const midi_usb_packet_t *packet);

/**
 * @brief Send MIDI 1.0 message via USB
 * 
 * Automatically converts to USB-MIDI 1.0 Event Packet
 * 
 * @param msg MIDI 1.0 message
 * @param cable_number Virtual cable (0-15)
 * @return ESP_OK on success
 */
esp_err_t midi_usb_send_midi1_message(const midi_message_t *msg, uint8_t cable_number);

/**
 * @brief Send UMP via USB
 * 
 * Requires MIDI 2.0 support enabled
 * 
 * @param ump UMP packet
 * @param cable_number Virtual cable (0-15)
 * @return ESP_OK on success
 */
esp_err_t midi_usb_send_ump(const ump_packet_t *ump, uint8_t cable_number);

/**
 * @brief Get current USB mode
 * 
 * @return Current USB operating mode
 */
midi_usb_mode_t midi_usb_get_mode(void);

/**
 * @brief Check if USB is connected
 * 
 * @return true if USB device/host connected
 */
bool midi_usb_is_connected(void);

/**
 * @brief Get USB MIDI statistics
 * 
 * @param stats Output statistics structure
 * @return ESP_OK on success
 */
esp_err_t midi_usb_get_stats(midi_usb_stats_t *stats);

/**
 * @brief Reset statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_usb_reset_stats(void);

#endif /* MIDI_USB_H */

