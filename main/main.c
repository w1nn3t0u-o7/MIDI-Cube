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
//#include "nvs_flash.h"

// MIDI Core
#include "midi_types.h"
#include "midi_router.h"
#include "ump_types.h"

// Transports
#include "midi_uart.h"
//#include "midi_usb.h"
//#include "midi_wifi.h"
//#include "midi_ethernet.h"

// Test suite (optional)
#include "test_midi_core.h"

static const char *TAG = "main";

// Enable/disable test mode
#define ENABLE_TEST_MODE  0

//=============================================================================
// Global Configuration
//=============================================================================

// Router input queue (shared by all transports)
QueueHandle_t g_router_input_queue = NULL;

// Transport enable flags (set via Kconfig or runtime)
#define ENABLE_UART      1
#define ENABLE_USB       1
#define ENABLE_WIFI      1
#define ENABLE_ETHERNET  1

//=============================================================================
// Transport RX Callbacks - Feed Router Queue
//=============================================================================

/**
 * @brief UART RX Callback
 * Called by UART driver when MIDI message received
 */
static void uart_rx_callback(const midi_message_t *msg, void *ctx) {
    // Create router packet
    midi_router_packet_t packet = {
        .source = MIDI_TRANSPORT_UART,
        .format = MIDI_FORMAT_1_0,
        .timestamp_us = esp_timer_get_time(),
        .data.midi1 = *msg
    };
    
    // Send to router (non-blocking to avoid UART ISR delays)
    if (xQueueSend(g_router_input_queue, &packet, 0) != pdTRUE) {
        // Queue full - drop packet (log in debug mode)
        ESP_LOGD(TAG, "Router queue full, UART packet dropped");
    }
}

/**
 * @brief USB RX Callback
 */
// static void usb_rx_callback(const midi_usb_packet_t *usb_pkt, void *ctx) {
//     midi_router_packet_t packet = {
//         .source = MIDI_TRANSPORT_USB,
//         .timestamp_us = usb_pkt->timestamp_us
//     };
    
//     // Check if MIDI 1.0 or 2.0
//     if (usb_pkt->protocol == MIDI_USB_PROTOCOL_1_0) {
//         packet.format = MIDI_FORMAT_1_0;
//         // Convert USB-MIDI to standard MIDI 1.0
//         packet.data.midi1.status = usb_pkt->data.midi1.midi_bytes[0];
//         packet.data.midi1.channel = usb_pkt->data.midi1.midi_bytes[0] & 0x0F;
//         packet.data.midi1.data.bytes[0] = usb_pkt->data.midi1.midi_bytes[1];
//         packet.data.midi1.data.bytes[1] = usb_pkt->data.midi1.midi_bytes[2];
//     } else {
//         packet.format = MIDI_FORMAT_2_0;
//         packet.data.ump = usb_pkt->data.ump;
//     }
    
//     xQueueSend(g_router_input_queue, &packet, 0);
// }

/**
 * @brief WiFi/Ethernet RX Callback (UMP packets)
 */
// static void network_rx_callback(const ump_packet_t *ump, void *ctx) {
//     midi_transport_t source = (midi_transport_t)(uintptr_t)ctx;
    
//     midi_router_packet_t packet = {
//         .source = source,
//         .format = MIDI_FORMAT_2_0,
//         .timestamp_us = esp_timer_get_time(),
//         .data.ump = *ump
//     };
    
//     xQueueSend(g_router_input_queue, &packet, 0);
// }

//=============================================================================
// MIDI Router Task - Core 0, Priority 10
//=============================================================================

/**
 * @brief Central MIDI Router Task
 * 
 * Runs on Core 0 (protocol core) at high priority.
 * Receives packets from all transports and routes them based on
 * routing matrix configuration.
 */
static void midi_router_task(void *pvParameters) {
    midi_router_packet_t packet;
    
    ESP_LOGI(TAG, "Router task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Wait for incoming packet from any transport
        if (xQueueReceive(g_router_input_queue, &packet, portMAX_DELAY) == pdTRUE) {
            
            // Route packet via router
            // Router handles:
            // - Routing matrix lookup
            // - Protocol translation (MIDI 1.0 ↔ MIDI 2.0)
            // - Filtering
            // - Output to destination transport(s)
            esp_err_t err = midi_router_route_packet(&packet);
            
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Router error: %s", esp_err_to_name(err));
            }
        }
    }
}

//=============================================================================
// Statistics Task - Core 1, Priority 3
//=============================================================================

/**
 * @brief Statistics and Monitoring Task
 */
static void stats_task(void *pvParameters) {
    ESP_LOGI(TAG, "Statistics task started on core %d", xPortGetCoreID());
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));  // Every 5 seconds
        
        // Get router stats
        midi_router_stats_t stats;
        midi_router_get_stats(&stats);
        
        // Log activity
        ESP_LOGI(TAG, "=== MIDI Activity (last 5s) ===");
        ESP_LOGI(TAG, "  UART:     %lu packets", stats.packets_routed[MIDI_TRANSPORT_UART]);
        ESP_LOGI(TAG, "  USB:      %lu packets", stats.packets_routed[MIDI_TRANSPORT_USB]);
        ESP_LOGI(TAG, "  WiFi:     %lu packets", stats.packets_routed[MIDI_TRANSPORT_WIFI]);
        ESP_LOGI(TAG, "  Ethernet: %lu packets", stats.packets_routed[MIDI_TRANSPORT_ETHERNET]);
        ESP_LOGI(TAG, "  Dropped:  %lu packets", stats.packets_dropped);
        
        // Check queue depth
        UBaseType_t queue_waiting = uxQueueMessagesWaiting(g_router_input_queue);
        ESP_LOGI(TAG, "  Queue depth: %u / 64", queue_waiting);
    }
}

//=============================================================================
// Initialization Functions
//=============================================================================

/**
 * @brief Initialize NVS (required for WiFi)
 */
// static void init_nvs(void) {
//     esp_err_t err = nvs_flash_init();
//     if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         err = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(err);
//     ESP_LOGI(TAG, "NVS initialized");
// }

/**
 * @brief Initialize MIDI Router
 */
static void init_router(void) {
    // Create router input queue (64 packets deep)
    g_router_input_queue = xQueueCreate(64, sizeof(midi_router_packet_t));
    if (!g_router_input_queue) {
        ESP_LOGE(TAG, "Failed to create router queue!");
        esp_restart();
    }
    
    // Configure router
    midi_router_config_t router_cfg = {
        .auto_translate = true,      // Auto MIDI 1.0 ↔ 2.0 translation
        .merge_inputs = false,       // Use routing matrix
        .default_group = 0,          // Default UMP group
        .enable_filtering = true     // Enable message filtering
    };
    
    // Set default routing matrix (all inputs → all outputs except loopback)
    for (int src = 0; src < MIDI_TRANSPORT_COUNT; src++) {
        for (int dest = 0; dest < MIDI_TRANSPORT_COUNT; dest++) {
            router_cfg.routing_matrix[src][dest] = (src != dest);
        }
    }
    
    ESP_ERROR_CHECK(midi_router_init(&router_cfg));
    ESP_LOGI(TAG, "MIDI Router initialized");
}

/**
 * @brief Initialize UART Transport
 */
static void init_uart(void) {
#if ENABLE_UART
    midi_uart_config_t uart_cfg = {
        .uart_num = UART_NUM_1,
        .tx_pin = CONFIG_MIDI_UART_TX_GPIO,
        .rx_pin = CONFIG_MIDI_UART_RX_GPIO,
        .baud_rate = 31250,
        .enable_tx = true,
        .enable_rx = true,
        .rx_callback = uart_rx_callback,
        .rx_callback_ctx = NULL
    };
    
    ESP_ERROR_CHECK(midi_uart_init(&uart_cfg));
    ESP_LOGI(TAG, "UART MIDI initialized (TX: GPIO%d, RX: GPIO%d)", 
             uart_cfg.tx_pin, uart_cfg.rx_pin);
#endif
}

/**
 * @brief Initialize USB Transport
 */
// static void init_usb(void) {
// #if ENABLE_USB
//     midi_usb_config_t usb_cfg = {
//         .mode = MIDI_USB_MODE_AUTO,  // Auto-detect device/host
//         .enable_midi2 = true,
//         .num_cables = 1,
//         .device_vid = 0x1234,        // TODO: Get real VID
//         .device_pid = 0x5678,
//         .rx_callback = usb_rx_callback,
//         .callback_ctx = NULL
//     };
    
//     ESP_ERROR_CHECK(midi_usb_init(&usb_cfg));
//     ESP_LOGI(TAG, "USB MIDI initialized");
// #endif
// }

/**
 * @brief Initialize WiFi Transport
 */
// static void init_wifi(void) {
// #if ENABLE_WIFI
//     midi_wifi_config_t wifi_cfg = {
//         .mode = MIDI_WIFI_MODE_HOST,
//         .host_port = CONFIG_MIDI_WIFI_HOST_UDP_PORT,
//         .enable_mdns = true,
//         .enable_fec = true,
//         .rx_callback = (midi_wifi_rx_callback_t)network_rx_callback,
//         .callback_ctx = (void*)MIDI_TRANSPORT_WIFI
//     };
//     strcpy(wifi_cfg.endpoint_name, CONFIG_MIDI_WIFI_UMP_ENDPOINT_NAME);
    
//     ESP_ERROR_CHECK(midi_wifi_init(&wifi_cfg));
    
//     // Connect to WiFi AP
//     #ifdef CONFIG_MIDI_WIFI_SSID
//     ESP_LOGI(TAG, "Connecting to WiFi: %s", CONFIG_MIDI_WIFI_SSID);
//     midi_wifi_connect(CONFIG_MIDI_WIFI_SSID, CONFIG_MIDI_WIFI_PASSWORD, 10000);
//     #endif
    
//     ESP_LOGI(TAG, "WiFi MIDI initialized");
// #endif
// }

/**
 * @brief Initialize Ethernet Transport
 */
// static void init_ethernet(void) {
// #if ENABLE_ETHERNET
//     midi_ethernet_config_t eth_cfg = {
//         .spi_host = CONFIG_MIDI_ETH_SPI_HOST,
//         .spi_clock_speed_mhz = CONFIG_MIDI_ETH_SPI_CLOCK_MHZ,
//         .gpio_sclk = CONFIG_MIDI_ETH_SPI_SCLK_GPIO,
//         .gpio_mosi = CONFIG_MIDI_ETH_SPI_MOSI_GPIO,
//         .gpio_miso = CONFIG_MIDI_ETH_SPI_MISO_GPIO,
//         .gpio_cs = CONFIG_MIDI_ETH_SPI_CS_GPIO,
//         .gpio_int = CONFIG_MIDI_ETH_SPI_INT_GPIO,
//         .use_dhcp = CONFIG_MIDI_ETH_USE_DHCP,
//         .host_port = CONFIG_MIDI_ETH_HOST_UDP_PORT,
//         .enable_mdns = true,
//         .rx_callback = (midi_ethernet_rx_callback_t)network_rx_callback,
//         .callback_ctx = (void*)MIDI_TRANSPORT_ETHERNET
//     };
//     strcpy(eth_cfg.endpoint_name, CONFIG_MIDI_ETH_UMP_ENDPOINT_NAME);
    
//     ESP_ERROR_CHECK(midi_ethernet_init(&eth_cfg));
    
//     // Wait for Ethernet link
//     ESP_LOGI(TAG, "Waiting for Ethernet link...");
//     midi_ethernet_wait_for_link(10000);
    
//     ESP_LOGI(TAG, "Ethernet MIDI initialized");
// #endif
// }

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
    // 1. System Initialization
    //=========================================================================
    init_nvs();
    init_router();
    
    //=========================================================================
    // 2. Transport Initialization
    //=========================================================================
    init_uart();
    init_usb();
    init_wifi();
    init_ethernet();
    
    //=========================================================================
    // 3. Create FreeRTOS Tasks
    //=========================================================================
    
    // Core 0: MIDI Router (high priority)
    xTaskCreatePinnedToCore(
        midi_router_task,
        "midi_router",
        8192,                    // Stack size
        NULL,
        10,                      // Priority (high)
        NULL,
        0                        // Core 0 (protocol core)
    );
    
    // Core 1: Statistics Task (low priority)
    xTaskCreatePinnedToCore(
        stats_task,
        "stats",
        3072,
        NULL,
        3,                       // Priority (low)
        NULL,
        1                        // Core 1
    );
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  System Running!");
    ESP_LOGI(TAG, "  Router: Core 0, Priority 10");
    ESP_LOGI(TAG, "  UI: Core 1, Priority 5");
    ESP_LOGI(TAG, "  Stats: Core 1, Priority 3");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // Main task can now delete itself or handle other duties
    // vTaskDelete(NULL);
}
