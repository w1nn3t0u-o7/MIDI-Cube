/**
 * @file midi_uart.c
 * @brief MIDI UART Driver Implementation
 */

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "midi_uart.h"
#include "midi_message.h"
#include "midi_router.h"

static const char *TAG = "midi_uart";

static midi_uart_driver_t uart_driver;

esp_err_t midi_uart_configure(QueueHandle_t *uart_event_queue)
{
    ESP_LOGI(TAG, "Configuring UART%d for MIDI 1.0", MIDI_UART_PORT);
    ESP_LOGI(TAG, "  Baud rate: %d", MIDI_UART_BAUD_RATE);
    ESP_LOGI(TAG, "  TX Pin: GPIO%d", MIDI_UART_TX_PIN);
    ESP_LOGI(TAG, "  RX Pin: GPIO%d", MIDI_UART_RX_PIN);
    
    // UART configuration
    uart_config_t uart_config = {
        .baud_rate = MIDI_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Configure UART parameters
    esp_err_t err = uart_param_config(MIDI_UART_PORT, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Set UART pins
    err = uart_set_pin(
        MIDI_UART_PORT,
        MIDI_UART_TX_PIN,       // TX
        MIDI_UART_RX_PIN,       // RX
        UART_PIN_NO_CHANGE,     // RTS (not used)
        UART_PIN_NO_CHANGE      // CTS (not used)
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Install UART driver with RX/TX buffers
    err = uart_driver_install(
        MIDI_UART_PORT,
        MIDI_UART_RX_BUF_SIZE,
        MIDI_UART_TX_BUF_SIZE,
        MIDI_UART_EVENT_QUEUE_SIZE,
        uart_event_queue,    // ← OUT parameter (v5.5 API change)
        0                        // Interrupt allocation flags
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "MIDI UART hardware configured successfully");
    ESP_LOGI(TAG, "  Event queue created: %p", (void*)uart_event_queue);
    
    return ESP_OK;
}

esp_err_t midi_uart_deconfigure(QueueHandle_t *uart_event_queue)
{
    esp_err_t err = uart_driver_delete(MIDI_UART_PORT);
    *uart_event_queue = NULL;  // Clear handle
    return err;
}

static void midi_uart_rx_task(void *arg)
{
    midi_uart_driver_t *driver = (midi_uart_driver_t *)arg;
    uint8_t rx_byte;
    midi_message_t msg;
    bool complete;
    uart_event_t event;
    QueueHandle_t uart_queue = driver->uart_event_queue;
    
    ESP_LOGI(TAG, "MIDI UART RX task started on core %d", xPortGetCoreID());
    
    if (uart_queue == NULL) {
        ESP_LOGE(TAG, "UART event queue is NULL! Cannot start RX task.");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGD(TAG, "RX callback: %p", driver->rx_callback);  // ← Check if NULL
    while (1) {
        // Wait for UART event
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG, "Event: type=%d, size=%d", event.type, event.size);
            switch (event.type) {
                case UART_DATA: {
                    uint8_t data[128];
                    int len = uart_read_bytes(MIDI_UART_PORT, data, 
                                              event.size, 0);
                    ESP_LOGD(TAG, "Read %d bytes: ", len);
                    if (len > 0) {
                        midi_uart_send_bytes(data, len);
                        // Process each byte
                        for (int i = 0; i < len; i++) {
                            rx_byte = data[i];
                                
                            // Feed byte to MIDI parser
                            esp_err_t err = midi1_parser_parse_byte(
                                &driver->parser,
                                rx_byte,
                                &msg,
                                &complete
                            );
                                
                            if (err != ESP_OK) {
                                ESP_LOGW(TAG, "Parser error for byte 0x%02X", rx_byte);
                                continue;
                            }
                                
                            // If message complete, call callback
                            if (complete) {
                                // Debug logging (verbose)
                                ESP_LOGD(TAG, "RX: Status=0x%02X, Ch=%d, D1=%d, D2=%d",
                                            msg.status_byte, MIDI1_MSG_GET_CHANNEL(&msg), msg.data[0], msg.data[1]);
                                // Call user callback
                                if (driver->rx_callback) {
                                    driver->rx_callback(&msg);
                                }
                            }
                        }
                    }
                }
                break;
                    
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART RX buffer full - flushing!");
                    uart_flush_input(MIDI_UART_PORT);
                    xQueueReset(uart_queue);
                    break;
                    
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART FIFO overflow!");
                    uart_flush_input(MIDI_UART_PORT);
                    xQueueReset(uart_queue);
                    break;
                    
                case UART_FRAME_ERR:
                    ESP_LOGW(TAG, "UART frame error");
                    break;
                    
                case UART_PARITY_ERR:
                    ESP_LOGW(TAG, "UART parity error");
                    break;
                    
                case UART_BREAK:
                    ESP_LOGD(TAG, "UART break detected");
                    break;
                    
                case UART_PATTERN_DET:
                    // Not used for MIDI, but handle gracefully
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown UART event type: %d", event.type);
                    break;
            }
        }
    }
}

esp_err_t midi_uart_init(void)
{
    uart_driver.is_initialized = false;

    if (uart_driver.is_initialized) {
        ESP_LOGW(TAG, "MIDI UART already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing MIDI UART driver");
    
    // Configure hardware
    esp_err_t err = midi_uart_configure(&uart_driver.uart_event_queue);
    if (err != ESP_OK) {
        return err;
    }
    
    // Initialize MIDI parser
    err = midi1_parser_init(&uart_driver.parser);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Parser init failed: %s", esp_err_to_name(err));
        midi_uart_deconfigure(&uart_driver.uart_event_queue);
        return err;
    }

    uart_driver.rx_callback = uart_rx_callback;

    // Create RX task
    BaseType_t task_created = xTaskCreatePinnedToCore(
        midi_uart_rx_task,
        "midi_uart_rx",
        MIDI_UART_TASK_STACK_SIZE,
        &uart_driver,
        MIDI_UART_TASK_PRIORITY,
        &uart_driver.rx_task_handle,
        MIDI_UART_TASK_CORE
    );
        
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        midi_uart_deconfigure(&uart_driver.uart_event_queue);
        return ESP_FAIL;
    }

    uart_driver.is_initialized = true;
    ESP_LOGI(TAG, "MIDI UART initialized successfully");
    
    return ESP_OK;
}

esp_err_t midi_uart_deinit(void)
{
    if (!uart_driver.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing MIDI UART driver");
    
    // Delete RX task
    if (uart_driver.rx_task_handle) {
        vTaskDelete(uart_driver.rx_task_handle);
        uart_driver.rx_task_handle = NULL;
    }
    
    // Deconfigure hardware
    midi_uart_deconfigure(&uart_driver.uart_event_queue);

    uart_driver.is_initialized = false;

    ESP_LOGI(TAG, "MIDI UART deinitialized");
    return ESP_OK;
}

esp_err_t midi_uart_send_message(const midi_message_t *msg)
{
    if (!uart_driver.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Serialize message to bytes
    uint8_t buffer[16];  // Max MIDI message size
    size_t bytes_written = 0;
    
    esp_err_t err = midi1_message_to_bytes(msg, buffer, sizeof(buffer), &bytes_written);
    if (err != ESP_OK) {
        return err;
    }
    
    // Send via UART
    int sent = uart_write_bytes(MIDI_UART_PORT, (const char *)buffer, bytes_written);
    
    if (sent == bytes_written) {
        return ESP_OK;
    } else if (sent < 0) {
        return ESP_FAIL;
    } else {
        // Partial write (shouldn't happen with sufficient buffer)
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t midi_uart_send_bytes(const uint8_t *data, size_t len)
{
    if (!uart_driver.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int sent = uart_write_bytes(MIDI_UART_PORT, (const char *)data, len);
    
    if (sent == len) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

bool midi_uart_is_initialized(void)
{
    return uart_driver.is_initialized;
}

esp_err_t midi_uart_flush_tx(uint32_t timeout_ms)
{
    if (!uart_driver.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = uart_wait_tx_done(MIDI_UART_PORT, pdMS_TO_TICKS(timeout_ms));
    return err;
}

