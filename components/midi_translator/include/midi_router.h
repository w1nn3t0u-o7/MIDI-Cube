/**
 * @file midi_router.h
 * @brief MIDI Router - Central Message Routing
 * 
 * Implements flexible, configurable routing between 4 transports:
 * - UART/DIN (MIDI 1.0)
 * - USB (MIDI 1.0)
 * - Ethernet (MIDI 2.0 over UDP)
 * - WiFi (MIDI 2.0 over UDP)
 */

#ifndef MIDI_ROUTER_H
#define MIDI_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "midi_message.h"
#include "ump_packet.h"
#include "midi_network.h"

/**
 * @defgroup MIDI_ROUTER_CONFIG Router Configuration
 * @{
 */
#define ROUTER_QUEUE_SIZE 64
#define ROUTER_TASK_STACK_SIZE 4096
#define ROUTER_TASK_PRIORITY 10
#define ROUTER_TASK_CORE 1

/** @} */

/**
 * @brief Transport identifiers
 */
typedef enum {
    MIDI_TRANSPORT_UART,  /**< UART/DIN-5 (MIDI 1.0) */
    MIDI_TRANSPORT_USB,       /**< USB (MIDI 1.0) */
    MIDI_TRANSPORT_NETWORK,  /**< Ethernet/WiFi (MIDI 2.0 over UDP) */
    MIDI_TRANSPORT_COUNT      /**< Number of transports */
} midi_transport_t;

/**
 * @brief MIDI format identifiers
 */
typedef enum {
    MIDI_FORMAT_1_0,  /**< MIDI 1.0 format */
    MIDI_FORMAT_2_0   /**< MIDI 2.0 format */
} midi_format_t;

/**
 * @brief Router state
 */
typedef struct midi_router_config {
    bool initialized;
    bool enable_uart;
    bool enable_usb;
    bool enable_network;
    bool uart_loopback;
    
    // Message queue
    QueueHandle_t packet_queue;
    
    // Router task
    TaskHandle_t router_task_handle;
    
} midi_router_config_t;

/**
 * @brief MIDI packet format (unified internal format)
 */
typedef struct midi_router_packet {
    midi_transport_t source;     /**< Source transport */
    uint8_t format;               /**< 0=MIDI1.0, 1=UMP */
    uint8_t cable;                /**< Cable number (for USB) */
    uint8_t cin;                  /**< Code Index Number (for MIDI 1.0) */
    
    union {
        midi1_message_t midi1;     /**< MIDI 1.0 message */
        ump_packet_t ump;         /**< UMP packet */
    } data;
} midi_router_packet_t;

/**
 * @brief Callback when MIDI message is received from UART
 * 
 * @param msg Parsed MIDI message
 */
void uart_rx_callback(const midi1_message_t *msg);

/**
 * @brief Callback when UMP packet is received from network
 * This sends the packet to the MIDI router for processing
 * 
 * @param session Pointer to MIDI network session
 * @param ump Pointer to received UMP packet
 */
void midi_net_rx_callback(midi_net_session_t *session, const ump_packet_t *ump);

/**
 * @brief Callback when MIDI message is received from USB
 * 
 * @param cable USB MIDI cable number
 * @param cin Code Index Number
 * @param msg Parsed MIDI message
 */
void midi_usbd_rx_callback(uint8_t cable, uint8_t cin, midi1_message_t *msg);

/**
 * @brief Initialize MIDI router
 * 
 * Creates router task, initializes buffers, loads configuration from NVS
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_router_init(void);

/**
 * @brief Deinitialize MIDI router
 * 
 * Stops router task, saves configuration to NVS, frees resources
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_router_deinit(void);

/**
 * @brief Get transport name string
 * 
 * @param transport Transport ID
 * @return Human-readable name (e.g., "UART")
 */
const char* midi_router_get_transport_name(midi_transport_t transport);

#endif /* MIDI_ROUTER_H */
