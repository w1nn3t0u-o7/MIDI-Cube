/**
 * @file midi_usb.c
 * @brief USB MIDI Driver - Main Controller (Complete)
 */

#include "midi_usb.h"
#include "midi_usb_device.h"
#include "midi_usb_host.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "midi_message.h"
#include <string.h>

static const char *TAG = "midi_usb";

// USB ID pin detection (for OTG mode switching)
#define USB_ID_PIN GPIO_NUM_4  // Typical ID pin on ESP32-S3

/**
 * @brief USB MIDI driver state
 */
typedef struct {
    bool initialized;
    midi_usb_config_t config;
    midi_usb_stats_t stats;
    midi_usb_mode_t active_mode;
} midi_usb_state_t;

static midi_usb_state_t g_usb_state = {0};

/**
 * @brief USB-MIDI 1.0 Code Index Number (CIN) lookup
 * Maps MIDI status byte to CIN for USB-MIDI Event Packets
 */
static uint8_t get_cin_from_status(uint8_t status) {
    if (status >= 0xF8) return 0x0F;      // Single byte system real-time
    if (status >= 0xF0) {
        if (status == 0xF0) return 0x04;  // SysEx start
        if (status == 0xF1 || status == 0xF3) return 0x02;  // 2-byte
        if (status == 0xF2) return 0x03;  // 3-byte (SPP)
        if (status == 0xF7) return 0x05;  // SysEx end (single byte)
        return 0x0F;                       // Other system common
    }
    
    uint8_t msg_type = (status >> 4) & 0x0F;
    switch (msg_type) {
        case 0x8: return 0x08;  // Note Off
        case 0x9: return 0x09;  // Note On
        case 0xA: return 0x0A;  // Poly Pressure
        case 0xB: return 0x0B;  // Control Change
        case 0xC: return 0x0C;  // Program Change
        case 0xD: return 0x0D;  // Channel Pressure
        case 0xE: return 0x0E;  // Pitch Bend
        default: return 0x0F;   // Reserved
    }
}

/**
 * @brief Get message length from CIN
 */
static uint8_t get_length_from_cin(uint8_t cin) {
    switch (cin) {
        case 0x00: return 0;  // Misc
        case 0x01: return 0;  // Cable event
        case 0x02: return 2;  // 2-byte system common
        case 0x03: return 3;  // 3-byte system common
        case 0x04: return 3;  // SysEx start
        case 0x05: return 1;  // SysEx end (1 byte)
        case 0x06: return 2;  // SysEx end (2 bytes)
        case 0x07: return 3;  // SysEx end (3 bytes)
        case 0x08: return 3;  // Note Off
        case 0x09: return 3;  // Note On
        case 0x0A: return 3;  // Poly Pressure
        case 0x0B: return 3;  // Control Change
        case 0x0C: return 2;  // Program Change
        case 0x0D: return 2;  // Channel Pressure
        case 0x0E: return 3;  // Pitch Bend
        case 0x0F: return 1;  // Single byte
        default: return 0;
    }
}

/**
 * @brief Detect USB mode via ID pin
 */
static midi_usb_mode_t detect_usb_mode(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << USB_ID_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    int level = gpio_get_level(USB_ID_PIN);
    ESP_LOGI(TAG, "USB ID pin level: %d", level);
    
    return (level == 0) ? MIDI_USB_MODE_HOST : MIDI_USB_MODE_DEVICE;
}

/**
 * @brief Initialize USB MIDI
 */
esp_err_t midi_usb_init(const midi_usb_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_usb_state.initialized) {
        ESP_LOGW(TAG, "USB MIDI already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing USB MIDI driver");
    
    memset(&g_usb_state, 0, sizeof(g_usb_state));
    g_usb_state.config = *config;
    
    // Determine USB mode
    midi_usb_mode_t mode = config->mode;
    if (mode == CONFIG_MIDI_USB_MODE_AUTO) {
        mode = detect_usb_mode();
        ESP_LOGI(TAG, "Auto-detected USB mode: %s",
                 (mode == MIDI_USB_MODE_HOST) ? "HOST" : "DEVICE");
    }
    
    g_usb_state.active_mode = mode;
    g_usb_state.stats.current_mode = mode;
    
    // Initialize appropriate mode
    esp_err_t err;
    if (mode == MIDI_USB_MODE_DEVICE) {
        err = midi_usb_device_init(config);
    } else {
        err = midi_usb_host_init(config);
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB %s mode: %s",
                 (mode == MIDI_USB_MODE_HOST) ? "Host" : "Device",
                 esp_err_to_name(err));
        return err;
    }
    
    g_usb_state.initialized = true;
    ESP_LOGI(TAG, "USB MIDI initialized in %s mode",
             (mode == MIDI_USB_MODE_HOST) ? "HOST" : "DEVICE");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize USB MIDI
 */
esp_err_t midi_usb_deinit(void) {
    if (!g_usb_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing USB MIDI");
    
    if (g_usb_state.active_mode == MIDI_USB_MODE_DEVICE) {
        midi_usb_device_deinit();
    } else {
        midi_usb_host_deinit();
    }
    
    g_usb_state.initialized = false;
    return ESP_OK;
}

/**
 * @brief Send USB MIDI packet
 */
esp_err_t midi_usb_send_packet(const midi_usb_packet_t *packet) {
    if (!g_usb_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_usb_state.active_mode == MIDI_USB_MODE_DEVICE) {
        return midi_usb_device_send_packet(packet);
    } else {
        return midi_usb_host_send_packet(packet);
    }
}

/**
 * @brief Send MIDI 1.0 message via USB
 */
esp_err_t midi_usb_send_midi1_message(const midi_message_t *msg, uint8_t cable_number) {
    if (!msg || cable_number > 15) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Convert to USB-MIDI 1.0 Event Packet (4 bytes)
    midi_usb_packet_t packet = {
        .cable_number = cable_number,
        .protocol = MIDI_USB_PROTOCOL_1_0,
        .timestamp_us = esp_timer_get_time()
    };
    
    // Build CIN from status
    uint8_t cin = get_cin_from_status(msg->status);
    packet.data.midi1.cin = cin;
    
    // Pack MIDI bytes
    packet.data.midi1.midi_bytes[0] = msg->status;
    packet.data.midi1.midi_bytes[1] = msg->data1;
    packet.data.midi1.midi_bytes[2] = msg->data2;
    
    return midi_usb_send_packet(&packet);
}

/**
 * @brief Send UMP via USB
 */
esp_err_t midi_usb_send_ump(const ump_packet_t *ump, uint8_t cable_number) {
    if (!ump || cable_number > 15) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_usb_state.config.enable_midi2) {
        ESP_LOGW(TAG, "MIDI 2.0 not enabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    midi_usb_packet_t packet = {
        .cable_number = cable_number,
        .protocol = MIDI_USB_PROTOCOL_2_0,
        .timestamp_us = esp_timer_get_time()
    };
    packet.data.ump = *ump;
    
    return midi_usb_send_packet(&packet);
}

/**
 * @brief Get current USB mode
 */
midi_usb_mode_t midi_usb_get_mode(void) {
    return g_usb_state.active_mode;
}

/**
 * @brief Check if USB is connected
 */
bool midi_usb_is_connected(void) {
    return g_usb_state.stats.connected;
}

/**
 * @brief Get USB MIDI statistics
 */
esp_err_t midi_usb_get_stats(midi_usb_stats_t *stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    *stats = g_usb_state.stats;
    return ESP_OK;
}

/**
 * @brief Reset statistics
 */
esp_err_t midi_usb_reset_stats(void) {
    memset(&g_usb_state.stats, 0, sizeof(midi_usb_stats_t));
    g_usb_state.stats.current_mode = g_usb_state.active_mode;
    return ESP_OK;
}
