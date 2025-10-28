/**
 * @file midi_uart.c
 * @brief MIDI UART Driver Implementation
 */

#include "midi_uart.h"
#include "midi_uart_config.h"
#include "midi_message.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "midi_uart";

// Hardware configuration (from midi_uart_hw.c)
extern esp_err_t midi_uart_hw_configure(void);
extern esp_err_t midi_uart_hw_deconfigure(void);

/**
 * @brief MIDI UART driver state
 */
typedef struct {
    bool initialized;
    bool enabled_tx;
    bool enabled_rx;
    
    // Parser state
    midi_parser_state_t parser;
    uint8_t sysex_buffer[1024];  // SysEx buffer
    
    // Callback
    midi_uart_rx_callback_t rx_callback;
    void *rx_callback_ctx;
    
    // FreeRTOS task
    TaskHandle_t rx_task_handle;
    
    // Statistics
    midi_uart_stats_t stats;
    
} midi_uart_state_t;

static midi_uart_state_t g_uart_state = {0};

/**
 * @brief UART RX Task
 * 
 * Continuously reads from UART, feeds to MIDI parser,
 * calls callback on complete messages
 */
static void midi_uart_rx_task(void *arg) {
    midi_uart_state_t *state = (midi_uart_state_t *)arg;
    uint8_t rx_byte;
    midi_message_t msg;
    bool complete;
    
    ESP_LOGI(TAG, "MIDI UART RX task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Read single byte from UART (blocking with timeout)
        int len = uart_read_bytes(MIDI_UART_PORT, &rx_byte, 1, 
                                   pdMS_TO_TICKS(100));
        
        if (len == 1) {
            state->stats.bytes_received++;
            
            // Feed byte to MIDI parser
            esp_err_t err = midi_parser_parse_byte(
                &state->parser,
                rx_byte,
                &msg,
                &complete
            );
            
            if (err != ESP_OK) {
                state->stats.parser_errors++;
                ESP_LOGW(TAG, "Parser error for byte 0x%02X", rx_byte);
                continue;
            }
            
            // If message complete, call callback
            if (complete) {
                state->stats.messages_received++;
                
                // Add timestamp
                msg.timestamp_us = esp_timer_get_time();
                
                // Call user callback
                if (state->rx_callback) {
                    state->rx_callback(&msg, state->rx_callback_ctx);
                }
                
                // Debug logging (verbose)
                ESP_LOGD(TAG, "RX: Status=0x%02X, Ch=%d, D1=%d, D2=%d",
                         msg.status, msg.channel, msg.data1, msg.data2);
            }
        } else if (len < 0) {
            // UART error
            state->stats.rx_errors++;
            ESP_LOGW(TAG, "UART read error: %d", len);
        }
        
        // Check for UART events (errors, buffer full, etc.)
        uart_event_t event;
        if (xQueueReceive(uart_get_event_queue(MIDI_UART_PORT), 
                         &event, 0) == pdTRUE) {
            switch (event.type) {
                case UART_BUFFER_FULL:
                    state->stats.rx_overruns++;
                    ESP_LOGW(TAG, "UART RX buffer full - data loss!");
                    uart_flush_input(MIDI_UART_PORT);
                    break;
                    
                case UART_FIFO_OVF:
                    state->stats.rx_overruns++;
                    ESP_LOGW(TAG, "UART FIFO overflow!");
                    uart_flush_input(MIDI_UART_PORT);
                    break;
                    
                case UART_FRAME_ERR:
                    state->stats.rx_errors++;
                    ESP_LOGW(TAG, "UART frame error");
                    break;
                    
                case UART_PARITY_ERR:
                    state->stats.rx_errors++;
                    ESP_LOGW(TAG, "UART parity error (shouldn't happen)");
                    break;
                    
                default:
                    break;
            }
        }
    }
}

/**
 * @brief Initialize MIDI UART driver
 */
esp_err_t midi_uart_init(const midi_uart_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_uart_state.initialized) {
        ESP_LOGW(TAG, "MIDI UART already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing MIDI UART driver");
    
    // Clear state
    memset(&g_uart_state, 0, sizeof(g_uart_state));
    
    // Configure hardware
    esp_err_t err = midi_uart_hw_configure();
    if (err != ESP_OK) {
        return err;
    }
    
    // Initialize MIDI parser
    err = midi_parser_init(&g_uart_state.parser,
                           g_uart_state.sysex_buffer,
                           sizeof(g_uart_state.sysex_buffer));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Parser init failed: %s", esp_err_to_name(err));
        midi_uart_hw_deconfigure();
        return err;
    }
    
    // Store configuration
    g_uart_state.enabled_tx = config->enable_tx;
    g_uart_state.enabled_rx = config->enable_rx;
    g_uart_state.rx_callback = config->rx_callback;
    g_uart_state.rx_callback_ctx = config->rx_callback_ctx;
    
    // Create RX task if RX enabled
    if (config->enable_rx) {
        BaseType_t task_created = xTaskCreatePinnedToCore(
            midi_uart_rx_task,
            "midi_uart_rx",
            MIDI_UART_TASK_STACK_SIZE,
            &g_uart_state,
            MIDI_UART_TASK_PRIORITY,
            &g_uart_state.rx_task_handle,
            MIDI_UART_TASK_CORE
        );
        
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create RX task");
            midi_uart_hw_deconfigure();
            return ESP_FAIL;
        }
    }
    
    g_uart_state.initialized = true;
    ESP_LOGI(TAG, "MIDI UART initialized (TX:%s, RX:%s)",
             config->enable_tx ? "ON" : "OFF",
             config->enable_rx ? "ON" : "OFF");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize MIDI UART driver
 */
esp_err_t midi_uart_deinit(void) {
    if (!g_uart_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing MIDI UART driver");
    
    // Delete RX task
    if (g_uart_state.rx_task_handle) {
        vTaskDelete(g_uart_state.rx_task_handle);
        g_uart_state.rx_task_handle = NULL;
    }
    
    // Deconfigure hardware
    midi_uart_hw_deconfigure();
    
    g_uart_state.initialized = false;
    
    ESP_LOGI(TAG, "MIDI UART deinitialized");
    return ESP_OK;
}

/**
 * @brief Send MIDI message
 */
esp_err_t midi_uart_send_message(const midi_message_t *msg) {
    if (!g_uart_state.initialized || !g_uart_state.enabled_tx) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Serialize message to bytes
    uint8_t buffer[16];  // Max MIDI message size
    size_t bytes_written = 0;
    
    esp_err_t err = midi_message_to_bytes(msg, buffer, sizeof(buffer), &bytes_written);
    if (err != ESP_OK) {
        return err;
    }
    
    // Send via UART
    int sent = uart_write_bytes(MIDI_UART_PORT, (const char *)buffer, bytes_written);
    
    if (sent == bytes_written) {
        g_uart_state.stats.bytes_transmitted += bytes_written;
        g_uart_state.stats.messages_transmitted++;
        return ESP_OK;
    } else if (sent < 0) {
        g_uart_state.stats.tx_overruns++;
        return ESP_FAIL;
    } else {
        // Partial write (shouldn't happen with sufficient buffer)
        g_uart_state.stats.tx_overruns++;
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * @brief Send raw MIDI bytes
 */
esp_err_t midi_uart_send_bytes(const uint8_t *data, size_t len) {
    if (!g_uart_state.initialized || !g_uart_state.enabled_tx) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int sent = uart_write_bytes(MIDI_UART_PORT, (const char *)data, len);
    
    if (sent == len) {
        g_uart_state.stats.bytes_transmitted += len;
        return ESP_OK;
    } else {
        g_uart_state.stats.tx_overruns++;
        return ESP_FAIL;
    }
}

/**
 * @brief Get statistics
 */
esp_err_t midi_uart_get_stats(midi_uart_stats_t *stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *stats = g_uart_state.stats;
    stats->parser_errors = g_uart_state.parser.parse_errors;
    
    return ESP_OK;
}

/**
 * @brief Reset statistics
 */
esp_err_t midi_uart_reset_stats(void) {
    memset(&g_uart_state.stats, 0, sizeof(midi_uart_stats_t));
    g_uart_state.parser.parse_errors = 0;
    return ESP_OK;
}

/**
 * @brief Check if initialized
 */
bool midi_uart_is_initialized(void) {
    return g_uart_state.initialized;
}

/**
 * @brief Flush TX buffer
 */
esp_err_t midi_uart_flush_tx(uint32_t timeout_ms) {
    if (!g_uart_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = uart_wait_tx_done(MIDI_UART_PORT, pdMS_TO_TICKS(timeout_ms));
    return err;
}

