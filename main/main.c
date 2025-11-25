/**
 * @file main.c
 * @brief MIDI Cube - Multi-Transport MIDI Router
 * 
 * ESP32-S3 based MIDI router supporting:
 * - UART (MIDI DIN 5-pin)
 * - USB (Device & Host modes)
 * - WiFi (Network MIDI 2.0)
 * - Ethernet (W5500, Network MIDI 2.0)
 * 
 * Architecture:
 * - Core 0: Protocol processing (RX tasks + Router)
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

// MIDI Core
#include "midi_types.h"
#include "midi_router.h"
#include "ump_types.h"

// Transports
#include "midi_uart.h"
#include "midi_usb_device.h"
#include "midi_wifi.h"
#include "midi_ethernet.h"
#include "midi_network.h"

static const char *TAG = "main";

//=============================================================================
// Global Configuration
//=============================================================================

// Transport enable flags (set via Kconfig or runtime)
// #define ENABLE_UART      1
// #define ENABLE_USB       1
// #define ENABLE_WIFI      1
// #define ENABLE_ETHERNET  1

static bool midi_initialized = false;

// Called when ANY network interface gets an IP
void on_network_ready(esp_netif_t *netif, const char *ip_addr)
{
    ESP_LOGI("main", "Network interface ready: %s", ip_addr);
    
    // Initialize MIDI network endpoint only once
    if (!midi_initialized) {
        ESP_LOGI("main", "Initializing MIDI network endpoint...");
        
        esp_err_t ret = midi_net_ep_init("MIDI Cube", "ESP32-S3-001", 5004);
        if (ret != ESP_OK) {
            ESP_LOGE("main", "Failed to initialize MIDI endpoint: %d", ret);
            return;
        }
        
        // Register mDNS service for discovery
        ret = midi_net_register_mdns("midi-cube");
        if (ret != ESP_OK) {
            ESP_LOGE("main", "Failed to register mDNS: %d", ret);
            return;
        }
        
        midi_initialized = true;
        ESP_LOGI("main", "MIDI network endpoint ready and discoverable!");
    } else {
        ESP_LOGI("main", "MIDI already initialized - second interface ready");
    }
}

void on_network_lost(void)
{
    ESP_LOGW("main", "Network interface lost");
    // Optionally handle disconnection
    // Note: Don't stop MIDI if another interface is still up
}

//=============================================================================
// Main Application Entry Point
//=============================================================================

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  MIDI Cube - Multi-Transport Router");
    ESP_LOGI(TAG, "  ESP32-S3 Dual Core");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
#if ENABLE_TEST_MODE
    // Run test suite instead of normal operation
    midi_core_run_tests();
    ESP_LOGI(TAG, "Test mode complete. Reboot to run application.");
    return;
#endif
    
    //=========================================================================
    // 2. Transport Initialization
    //=========================================================================
    // Initialize WiFi (registers callbacks)
    midi_wifi_register_callbacks(on_network_ready, on_network_lost);
    midi_eth_init();
    midi_wifi_init();
    
    // Initialize Ethernet (registers callbacks)

    midi_router_init();
    midi_usbd_register_rx_callback(midi_usbd_rx_callback);
    midi_usbd_init();
    midi_uart_init();

    
    // Main task can now delete itself or handle other duties
    // vTaskDelete(NULL);
}
