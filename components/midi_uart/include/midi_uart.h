/**
 * @file midi_uart.h
 * @brief MIDI UART Transport Layer
 * 
 * Implements MIDI 1.0 over UART (DIN-5 connector)
 * - 31,250 baud, 8N1 (8 data bits, no parity, 1 stop bit)
 * - Full-duplex (simultaneous TX/RX)
 * - Hardware optocoupler support (6N138)
 * - Zero-copy ring buffer integration
 * 
 * Hardware Requirements:
 * - MIDI IN: 6N138 optocoupler + 220Ω + 270Ω resistors
 * - MIDI OUT: 220Ω resistor (current limiting)
 * - DIN-5 connectors
 * 
 * See: MIDI 1.0 Electrical Specification (ca33 5 PIn DIN)
 */

#ifndef MIDI_UART_H
#define MIDI_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "midi_types.h"
#include "midi_parser.h"

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
typedef struct {
    midi_uart_rx_callback_t rx_callback;  /**< RX callback function */
    void *rx_callback_ctx;                 /**< User context for callback */
    bool enable_tx;                        /**< Enable MIDI OUT (TX) */
    bool enable_rx;                        /**< Enable MIDI IN (RX) */
} midi_uart_config_t;

/**
 * @brief MIDI UART statistics
 */
typedef struct {
    uint32_t bytes_received;       /**< Total bytes received */
    uint32_t bytes_transmitted;    /**< Total bytes transmitted */
    uint32_t messages_received;    /**< Complete messages parsed */
    uint32_t messages_transmitted; /**< Messages sent */
    uint32_t rx_errors;            /**< RX errors (framing, parity, etc.) */
    uint32_t tx_overruns;          /**< TX buffer overruns */
    uint32_t rx_overruns;          /**< RX buffer overruns */
    uint32_t parser_errors;        /**< Parser errors */
} midi_uart_stats_t;

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
esp_err_t midi_uart_init(const midi_uart_config_t *config);

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
 * @brief Get MIDI UART statistics
 * 
 * @param stats Pointer to statistics structure to fill
 * @return ESP_OK on success
 */
esp_err_t midi_uart_get_stats(midi_uart_stats_t *stats);

/**
 * @brief Reset MIDI UART statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_uart_reset_stats(void);

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
