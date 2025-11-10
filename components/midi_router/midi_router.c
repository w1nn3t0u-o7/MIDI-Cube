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

// User callback that handles USB MIDI packets and sends to router queue
void midi_usbd_rx_callback(uint8_t cable, uint8_t cin, midi_message_t *msg)
{
    
    // Create MIDI packet structure
    midi_router_packet_t packet = {
        .source = MIDI_TRANSPORT_USB,
        .format = MIDI_FORMAT_1_0,
        .data.midi1 = *msg,  
        .cable = cable,
        .cin = cin,  // Extract Code Index Number from status byte
    };
    
    // Send to router queue (non-blocking)
    if (xQueueSend(router_state.packet_queue, &packet, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Router queue full - packet dropped [cable:%d]", packet.cable);
    } else {
        ESP_LOGD(TAG, "Packet sent to router [cable:%d]: %02X %02X %02X", 
                 packet.cable, packet.data.midi1.status, packet.data.midi1.data.bytes[0], packet.data.midi1.data.bytes[1]);
    }
}

/**
 * @brief Callback when UMP packet is received from network
 * This sends the packet to the MIDI router for processing
 */
void midi_net_rx_callback(midi_net_session_t *session, const ump_packet_t *ump)
{
    // Create routing message
    midi_router_packet_t msg = {
        .source = MIDI_TRANSPORT_NETWORK,  // Or however you identify network source
        .format = MIDI_FORMAT_2_0,
        .data.ump = *ump,
    };
    
    // Send to router queue (non-blocking)
    if (xQueueSend(router_state.packet_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Router queue full, dropped UMP packet");
    } else {
        ESP_LOGI(TAG, "Sent UMP to router: type=0x%X", 
                (ump->words[0] >> 28) & 0x0F);
    }
}


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
        
        if (packet.source == MIDI_TRANSPORT_UART) {
            ESP_LOGI(TAG, "Router received UART packet: Status=0x%02X", 
                     packet.data.midi1.status);
            // send it to UART TX
            // send it through USB
            // translate it and send it through network
        } else if (packet.source == MIDI_TRANSPORT_USB) {
            ESP_LOGI(TAG, "Router received USB packet: Status=0x%02X", 
                     packet.data.midi1.status);
            // send it through UART
            // translate it and send it through network
        } else if (packet.source == MIDI_TRANSPORT_NETWORK) {
            ESP_LOGI(TAG, "Router received Network UMP packet: Type=0x%X", 
                     (packet.data.ump.words[0] >> 28) & 0x0F);
            // translate it and send it through UART
            // send it through USB
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

