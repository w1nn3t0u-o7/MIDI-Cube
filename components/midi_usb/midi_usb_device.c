/**
 * @file midi_usb_device.c
 * @brief USB MIDI Device Mode Implementation
 * 
 * Uses TinyUSB for USB device stack
 */

#include "midi_usb_device.h"
#include "midi_usb_descriptors.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "tinyusb.h"
//#include "tusb.h"
#include "class/midi/midi_device.h"
#include <string.h>

static const char *TAG = "usb_device";

/**
 * @brief USB Device state
 */
typedef struct {
    bool initialized;
    bool mounted;
    midi_usb_config_t config;
    
    // TinyUSB MIDI interface
    uint8_t midi_itf;
    
    // Callbacks
    midi_usb_rx_callback_t rx_callback;
    midi_usb_conn_callback_t conn_callback;
    void *callback_ctx;
    
    // RX processing task
    TaskHandle_t rx_task_handle;
    
    // TX synchronization
    SemaphoreHandle_t tx_mutex;
    
} midi_usb_device_state_t;

static midi_usb_device_state_t g_device_state = {0};

/**
 * @brief TinyUSB MIDI RX callback
 * 
 * Called by TinyUSB when MIDI data received from PC
 */
static void tud_midi_rx_cb(uint8_t itf) {
    (void)itf;
    // Signal RX task that data is available
    if (g_device_state.rx_task_handle) {
        xTaskNotifyGive(g_device_state.rx_task_handle);
    }
}

/**
 * @brief TinyUSB mount callback
 * 
 * Called when USB device successfully enumerated by PC
 */
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB device mounted (PC connected)");
    g_device_state.mounted = true;
    
    if (g_device_state.conn_callback) {
        g_device_state.conn_callback(true, g_device_state.callback_ctx);
    }
}

/**
 * @brief TinyUSB unmount callback
 * 
 * Called when USB cable disconnected
 */
void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB device unmounted (PC disconnected)");
    g_device_state.mounted = false;
    
    if (g_device_state.conn_callback) {
        g_device_state.conn_callback(false, g_device_state.callback_ctx);
    }
}

/**
 * @brief Parse USB-MIDI 1.0 Event Packet to internal format
 */
static esp_err_t parse_usb_midi1_packet(const uint8_t *usb_packet,
                                         midi_usb_packet_t *out_packet) {
    // USB-MIDI 1.0 Event Packet structure (4 bytes):
    // Byte 0: [Cable Number (4 bits) | CIN (4 bits)]
    // Byte 1-3: MIDI bytes (up to 3 bytes)
    
    uint8_t header = usb_packet[0];
    out_packet->cable_number = (header >> 4) & 0x0F;
    out_packet->protocol = MIDI_USB_PROTOCOL_1_0;
    out_packet->timestamp_us = esp_timer_get_time();
    
    out_packet->data.midi1.cin = header & 0x0F;
    out_packet->data.midi1.midi_bytes[0] = usb_packet[1];
    out_packet->data.midi1.midi_bytes[1] = usb_packet[2];
    out_packet->data.midi1.midi_bytes[2] = usb_packet[3];
    
    return ESP_OK;
}

/**
 * @brief Parse UMP from USB
 */
static esp_err_t parse_usb_ump(const uint8_t *usb_data, size_t len,
                                midi_usb_packet_t *out_packet) {
    if (len < 4) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // UMP packets are 32-bit word aligned
    // Copy words into UMP structure
    uint32_t *words = (uint32_t *)usb_data;
    
    out_packet->protocol = MIDI_USB_PROTOCOL_2_0;
    out_packet->timestamp_us = esp_timer_get_time();
    
    // Determine packet size from Message Type
    uint8_t mt = (words[0] >> 28) & 0x0F;
    uint8_t num_words = 1;
    
    if (mt <= 0x2) num_words = 1;       // 32-bit
    else if (mt <= 0x5) num_words = 2;  // 64-bit
    else if (mt <= 0xC) num_words = 3;  // 96-bit
    else num_words = 4;                  // 128-bit
    
    if (len < (num_words * 4)) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Copy UMP words
    for (int i = 0; i < num_words && i < 4; i++) {
        out_packet->data.ump.words[i] = words[i];
    }
    out_packet->data.ump.num_words = num_words;
    out_packet->data.ump.message_type = mt;
    out_packet->data.ump.group = (words[0] >> 24) & 0x0F;
    
    return ESP_OK;
}

/**
 * @brief USB RX processing task
 * 
 * Continuously reads from TinyUSB MIDI class, parses packets,
 * calls user callback
 */
static void midi_usb_device_rx_task(void *arg) {
    ESP_LOGI(TAG, "USB Device RX task started");
    
    uint8_t buffer[64];
    midi_usb_packet_t packet;
    
    while (1) {
        // Wait for notification from TinyUSB RX callback
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        
        // Process all available packets
        while (tud_midi_available()) {
            uint32_t bytes_read = tud_midi_stream_read(buffer, sizeof(buffer));
            
            if (bytes_read == 0) {
                break;
            }
            
            ESP_LOGD(TAG, "RX: %lu bytes from PC", bytes_read);
            
            // Check protocol (MIDI 1.0 vs MIDI 2.0)
            // In MIDI 1.0 mode, packets are 4 bytes each
            // In MIDI 2.0 mode (UMP), packets are variable length
            
            if (g_device_state.config.enable_midi2) {
                // Parse as UMP
                esp_err_t err = parse_usb_ump(buffer, bytes_read, &packet);
                if (err == ESP_OK && g_device_state.rx_callback) {
                    g_device_state.rx_callback(&packet, g_device_state.callback_ctx);
                }
            } else {
                // Parse as USB-MIDI 1.0 (4 bytes per packet)
                for (size_t i = 0; i + 3 < bytes_read; i += 4) {
                    esp_err_t err = parse_usb_midi1_packet(&buffer[i], &packet);
                    if (err == ESP_OK && g_device_state.rx_callback) {
                        g_device_state.rx_callback(&packet, g_device_state.callback_ctx);
                    }
                }
            }
        }
    }
}

/**
 * @brief TinyUSB device task
 * 
 * Handles USB device stack processing
 */
static void tinyusb_device_task(void *arg) {
    ESP_LOGI(TAG, "TinyUSB device task started");
    
    while (1) {
        tud_task();  // TinyUSB device task
        vTaskDelay(1);  // Yield to other tasks
    }
}

/**
 * @brief Initialize USB Device mode
 */
esp_err_t midi_usb_device_init(const midi_usb_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_device_state.initialized) {
        ESP_LOGW(TAG, "USB device already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing USB Device mode");
    
    // Clear state
    memset(&g_device_state, 0, sizeof(g_device_state));
    g_device_state.config = *config;
    g_device_state.rx_callback = config->rx_callback;
    g_device_state.conn_callback = config->conn_callback;
    g_device_state.callback_ctx = config->callback_ctx;
    
    // Create TX mutex
    g_device_state.tx_mutex = xSemaphoreCreateMutex();
    if (!g_device_state.tx_mutex) {
        ESP_LOGE(TAG, "Failed to create TX mutex");
        return ESP_FAIL;
    }
    
    // Initialize TinyUSB
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = midi_usb_get_device_descriptor(),
        .string_descriptor = midi_usb_get_string_descriptors(),
        .external_phy = false,
        .configuration_descriptor = midi_usb_get_configuration_descriptor(config->enable_midi2),
    };
    
    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB driver install failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(g_device_state.tx_mutex);
        return err;
    }
    
    // Create TinyUSB task
    TaskHandle_t tusb_task;
    BaseType_t task_created = xTaskCreatePinnedToCore(
        tinyusb_device_task,
        "tinyusb_dev",
        4096,
        NULL,
        5,  // Lower priority than MIDI RX task
        &tusb_task,
        0   // Core 0 (USB runs on core 0)
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TinyUSB task");
        tinyusb_driver_uninstall();
        vSemaphoreDelete(g_device_state.tx_mutex);
        return ESP_FAIL;
    }
    
    // Create RX processing task
    task_created = xTaskCreatePinnedToCore(
        midi_usb_device_rx_task,
        "midi_usb_rx",
        4096,
        NULL,
        10,  // High priority for MIDI processing
        &g_device_state.rx_task_handle,
        1    // Core 1 (app core)
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        vTaskDelete(tusb_task);
        tinyusb_driver_uninstall();
        vSemaphoreDelete(g_device_state.tx_mutex);
        return ESP_FAIL;
    }
    
    g_device_state.initialized = true;
    
    ESP_LOGI(TAG, "USB Device mode initialized");
    ESP_LOGI(TAG, "  MIDI 2.0: %s", config->enable_midi2 ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "  Cables: %d", config->num_cables);
    ESP_LOGI(TAG, "Waiting for PC connection...");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize USB Device mode
 */
esp_err_t midi_usb_device_deinit(void) {
    if (!g_device_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing USB Device mode");
    
    // Delete tasks
    if (g_device_state.rx_task_handle) {
        vTaskDelete(g_device_state.rx_task_handle);
    }
    
    // Uninstall TinyUSB
    tinyusb_driver_uninstall();
    
    // Delete mutex
    if (g_device_state.tx_mutex) {
        vSemaphoreDelete(g_device_state.tx_mutex);
    }
    
    g_device_state.initialized = false;
    
    return ESP_OK;
}

/**
 * @brief Send packet in Device mode
 */
esp_err_t midi_usb_device_send_packet(const midi_usb_packet_t *packet) {
    if (!g_device_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_device_state.mounted) {
        ESP_LOGD(TAG, "USB not mounted, cannot send");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Take mutex
    if (xSemaphoreTake(g_device_state.tx_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t result = ESP_OK;
    
    if (packet->protocol == MIDI_USB_PROTOCOL_1_0) {
        // Send USB-MIDI 1.0 Event Packet (4 bytes)
        uint8_t usb_packet[4] = {
            (packet->cable_number << 4) | packet->data.midi1.cin,
            packet->data.midi1.midi_bytes[0],
            packet->data.midi1.midi_bytes[1],
            packet->data.midi1.midi_bytes[2]
        };
        
        uint32_t written = tud_midi_stream_write(packet->cable_number, 
                                                   usb_packet, 4);
        if (written != 4) {
            ESP_LOGW(TAG, "TX: Only wrote %lu/4 bytes", written);
            result = ESP_ERR_TIMEOUT;
        }
    } else {
        // Send UMP
        uint32_t num_bytes = packet->data.ump.num_words * 4;
        uint32_t written = tud_midi_stream_write(packet->cable_number,
                                                   (uint8_t*)packet->data.ump.words,
                                                   num_bytes);
        if (written != num_bytes) {
            ESP_LOGW(TAG, "UMP TX: Only wrote %lu/%lu bytes", written, num_bytes);
            result = ESP_ERR_TIMEOUT;
        }
    }
    
    // Release mutex
    xSemaphoreGive(g_device_state.tx_mutex);
    
    return result;
}

/**
 * @brief Check if USB device is mounted
 */
bool midi_usb_device_is_mounted(void) {
    return g_device_state.mounted;
}

/**
 * @brief Flush TX FIFO
 */
esp_err_t midi_usb_device_flush(uint32_t timeout_ms) {
    if (!g_device_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // TinyUSB handles flushing internally
    // Just ensure all data is written
    vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    
    return ESP_OK;
}
