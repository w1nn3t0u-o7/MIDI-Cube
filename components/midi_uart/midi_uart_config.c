/**
 * @file midi_uart_hw.c
 * @brief MIDI UART Hardware Configuration
 */

#include "midi_uart_config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "midi_uart_hw";

/**
 * @brief Configure UART hardware for MIDI
 * 
 * MIDI 1.0 Electrical Specification:
 * - 31,250 baud (312.5 × 100)
 * - 8 data bits
 * - No parity
 * - 1 stop bit
 * - Asynchronous (no clock)
 */
esp_err_t midi_uart_hw_configure(void) {
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
        MIDI_UART_TX_PIN,  // TX
        MIDI_UART_RX_PIN,  // RX
        UART_PIN_NO_CHANGE, // RTS (not used)
        UART_PIN_NO_CHANGE  // CTS (not used)
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
        NULL,  // Event queue handle (handled in main driver)
        0      // Interrupt allocation flags
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "MIDI UART hardware configured successfully");
    return ESP_OK;
}

/**
 * @brief Deconfigure UART hardware
 */
esp_err_t midi_uart_hw_deconfigure(void) {
    return uart_driver_delete(MIDI_UART_PORT);
}
