/**
 * @file midi_router.c
 * @brief MIDI Router Implementation
 */

#include "midi_router.h"
#include "midi_translator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "midi_router";

static midi_router_state_t router_state;

/**
 * @brief UART RX Callback
 * Called by UART driver when MIDI message received
 */
void uart_rx_callback(const midi_message_t *msg, void *ctx) {
    ESP_LOGI(TAG, "UART RX callback: Status=0x%02X, Ch=%d", msg->status, msg->channel);
    if (!router_state.packet_queue) {
        ESP_LOGW(TAG, "Router input queue not initialized");
        return;
    }
    // Create router packet
    midi_router_packet_t packet = {
        .source = MIDI_TRANSPORT_UART,
        .format = MIDI_FORMAT_1_0,
        .data.midi1 = *msg
    };

    // Send to router (non-blocking to avoid UART ISR delays)
    if (xQueueSend(router_state.packet_queue, &packet, 0) != pdTRUE) {
        // Queue full - drop packet (log in debug mode)
        ESP_LOGD(TAG, "Router queue full, UART packet dropped");
    }
}

/**
 * @brief Translate packet if needed
 */
static esp_err_t midi_router_translate(midi_router_packet_t *packet, 
                                        bool dest_wants_ump) {
    
    bool src_is_midi1 = (packet->format == MIDI_FORMAT_1_0);
    
    if (src_is_midi1 && dest_wants_ump) {
        // MIDI 1.0 → UMP
        ump_packet_t ump;
        esp_err_t err = midi_translate_1to2(&packet->data.midi1, &ump);
        if (err == ESP_OK) {
            packet->format = MIDI_FORMAT_2_0;
            packet->data.ump = ump;
        }
        return err;
    } else if (!src_is_midi1 && !dest_wants_ump) {
        // UMP → MIDI 1.0
        midi_message_t midi1;
        esp_err_t err = midi_translate_2to1(&packet->data.ump, &midi1);
        if (err == ESP_OK) {
            packet->format = MIDI_FORMAT_1_0;
            packet->data.midi1 = midi1;
        }
        return err;
    }
    
    return ESP_OK;  // No translation needed
}

/**
 * @brief Router task - processes incoming packets
 */
static void midi_router_task(void *arg) {
    midi_router_packet_t packet;
    
    ESP_LOGI(TAG, "Router task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Wait for packet
        if (xQueueReceive(router_state.packet_queue, &packet, 
                         portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        for (int dest = 0; dest < MIDI_TRANSPORT_COUNT; dest++) {
            
            // Don't route back to source (avoid loops)
            if (dest == packet.source) {
                continue;
            }
            
            // Translate if destination requires different format
            midi_router_packet_t out_packet = packet;
            bool dest_wants_ump = (dest == MIDI_TRANSPORT_ETHERNET || 
                                   dest == MIDI_TRANSPORT_WIFI ||
                                   dest == MIDI_TRANSPORT_USB);  // USB can do both
            // If needed translate
            esp_err_t err = midi_router_translate(&out_packet, dest_wants_ump);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Translation failed");
                continue;
            }
            // Send to destination
        }
    }
}

/**
 * @brief Initialize router
 */
esp_err_t midi_router_init(void) {
    if (router_state.initialized) {
        ESP_LOGW(TAG, "Router already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing MIDI router");
    
    // Clear state
    memset(&router_state, 0, sizeof(router_state));
    
    // Create packet queue
    router_state.packet_queue = xQueueCreate(ROUTER_QUEUE_SIZE, 
                                                sizeof(midi_router_packet_t));
    if (!router_state.packet_queue) {
        ESP_LOGE(TAG, "Failed to create packet queue");
        return ESP_FAIL;
    }
    
    // Create router task
    BaseType_t task_created = xTaskCreatePinnedToCore(
        midi_router_task,
        "midi_router",
        ROUTER_TASK_STACK_SIZE,
        NULL,
        ROUTER_TASK_PRIORITY,
        &router_state.router_task_handle,
        ROUTER_TASK_CORE
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create router task");
        vQueueDelete(router_state.packet_queue);
        return ESP_FAIL;
    }
    
    router_state.initialized = true;
    ESP_LOGI(TAG, "MIDI router initialized");
    
    return ESP_OK;
}

// ... (additional functions: set_route, get_route, save_config, etc.)
// [Implementation continues with NVS operations, config management]

