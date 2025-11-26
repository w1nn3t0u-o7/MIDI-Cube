/**
 * @file main.c
 * @brief MIDI Cube - Multi-Transport MIDI Router
 * 
 * ESP32-S3 based MIDI router supporting:
 * - UART (MIDI DIN 5-pin)
 * - USB (Device mode)
 * - WiFi (Network MIDI 2.0)
 * - Ethernet (W5500, Network MIDI 2.0)
 */

#include "esp_log.h"

#include "midi_router.h"
#include "midi_uart.h"
#include "midi_usb_device.h"
#include "midi_wifi.h"
#include "midi_ethernet.h"
#include "midi_network.h"

static const char *TAG = "main";

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

void app_main(void) {

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  MIDI Cube - Multi-Transport Router");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    midi_wifi_register_callbacks(on_network_ready, on_network_lost);
    midi_eth_init();
    midi_wifi_init();

    midi_router_init();
    midi_usbd_register_rx_callback(midi_usbd_rx_callback);
    midi_usbd_init();
    midi_uart_init();
}
