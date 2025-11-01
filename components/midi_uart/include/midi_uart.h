/**
 * @file midi_uart.h
 * @brief MIDI UART Transport Layer
 * 
 * Implements MIDI 1.0 over UART (DIN-5 connector)
 * - 31,250 baud, 8N1 (8 data bits, no parity, 1 stop bit)
 * - Full-duplex (simultaneous TX/RX)
 * - Hardware optocoupler support (H11L1)
 * - Zero-copy ring buffer integration
 * 
 * See: MIDI 1.0 Electrical Specification (ca33 5 PIn DIN)
 */

#ifndef MIDI_UART_H
#define MIDI_UART_H

//#include <stdint.h>
//#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "midi_types.h"
#include "midi_parser.h"
#include "sdkconfig.h"

/**
 * @defgroup MIDI_UART_HW Hardware Configuration
 * @{
 */

// MIDI Standard Baud Rate (MIDI 1.0 Spec)
#define MIDI_UART_BAUD_RATE         31250

// UART Configuration
#define MIDI_UART_PORT              CONFIG_MIDI_UART_PORT_NUM
#define MIDI_UART_TX_PIN            CONFIG_MIDI_UART_TX_PIN
#define MIDI_UART_RX_PIN            CONFIG_MIDI_UART_RX_PIN

// Buffer Sizes
#define MIDI_UART_RX_BUF_SIZE       CONFIG_MIDI_UART_RX_BUFFER_SIZE
#define MIDI_UART_TX_BUF_SIZE       CONFIG_MIDI_UART_TX_BUFFER_SIZE
#define MIDI_UART_EVENT_QUEUE_SIZE  CONFIG_MIDI_UART_EVENT_QUEUE_SIZE

// Task Configuration
#define MIDI_UART_TASK_STACK_SIZE   CONFIG_MIDI_UART_TASK_STACK_SIZE
#define MIDI_UART_TASK_PRIORITY     CONFIG_MIDI_UART_TASK_PRIORITY
#define MIDI_UART_TASK_CORE         1  // Run on Core 1 (Core 0 = WiFi)

/** @} */

/**
 * @brief MIDI UART callback function type
 * 
 * Called when complete MIDI message received from UART
 * 
 * @param msg Parsed MIDI message
 * @param user_ctx User context pointer
 */
typedef void (*midi_uart_rx_callback_t)(const midi_message_t *msg, void *user_ctx);

/**
 * @brief MIDI UART configuration
 */
// typedef struct {
//     midi_uart_rx_callback_t rx_callback;  /**< RX callback function */
//     void *rx_callback_ctx;                 /**< User context for callback */
//     bool enable_tx;                        /**< Enable MIDI OUT (TX) */
//     bool enable_rx;                        /**< Enable MIDI IN (RX) */
// } midi_uart_config_t;

/**
 * @brief MIDI UART driver state
 */
typedef struct {
    bool is_initialized;
    
    // Parser state
    midi_parser_state_t parser;
    uint8_t sysex_buffer[1024];  // SysEx buffer
    
    // Callback
    midi_uart_rx_callback_t rx_callback;
    void *rx_callback_ctx;
    
    // FreeRTOS task
    TaskHandle_t rx_task_handle;

    // UART event queue handle
    QueueHandle_t uart_event_queue;
    
} midi_uart_state_t;

esp_err_t midi_uart_configure(QueueHandle_t *uart_event_queue);

esp_err_t midi_uart_deconfigure(QueueHandle_t *uart_event_queue);

/**
 * @brief Initialize MIDI UART driver
 * 
 * Configures UART hardware for MIDI 1.0:
 * - Baud rate: 31,250
 * - 8 data bits, no parity, 1 stop bit (8N1)
 * - Installs UART driver with ISR
 * - Creates RX processing task
 * 
 * @param config MIDI UART configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t midi_uart_init(void);

/**
 * @brief Deinitialize MIDI UART driver
 * 
 * Stops RX task, uninstalls UART driver, frees resources
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_uart_deinit(void);

/**
 * @brief Send MIDI message over UART
 * 
 * Serializes MIDI message and transmits via UART TX.
 * Non-blocking - queues data to UART driver.
 * 
 * @param msg MIDI message to send
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if TX buffer full
 */
esp_err_t midi_uart_send_message(const midi_message_t *msg);

/**
 * @brief Send raw MIDI bytes over UART
 * 
 * Low-level transmission for pre-serialized MIDI data.
 * 
 * @param data Pointer to MIDI byte data
 * @param len Number of bytes to send
 * @return ESP_OK on success
 */
esp_err_t midi_uart_send_bytes(const uint8_t *data, size_t len);

/**
 * @brief Check if MIDI UART is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool midi_uart_is_initialized(void);

/**
 * @brief Flush UART TX buffer
 * 
 * Waits for all queued data to be transmitted
 * 
 * @param timeout_ms Timeout in milliseconds (0 = no wait)
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if timeout
 */
esp_err_t midi_uart_flush_tx(uint32_t timeout_ms);

#endif /* MIDI_UART_H */
