/**
 * @file midi_router.c
 * @brief MIDI Router Implementation
 */

#include "midi_router.h"
#include "midi_translator.h"
#include "midi_uart.h"
#include "midi_usb_device.h"
#include "midi_network.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "midi_router";

static midi_router_config_t router_state;
extern midi_net_ep_t midi_server;  // From midi_network component

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
                 packet.cable, packet.data.midi1.status, packet.data.midi1.data[0], packet.data.midi1.data[1]);
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
static void midi_router_task(void *arg) 
{
    midi_router_packet_t packet;
    midi_message_t translated_msg;
    ump_packet_t translated_ump;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Router task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Wait for packet from any source
        if (xQueueReceive(router_state.packet_queue, &packet, 
                         portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        // Route based on source
        switch (packet.source) {
            
            // ============================================================
            // UART MIDI Input - Route to USB and Network
            // ============================================================
            case MIDI_TRANSPORT_UART: {
                ESP_LOGD(TAG, "UART→Router: Status=0x%02X Ch=%d Data=[0x%02X 0x%02X]", 
                         packet.data.midi1.status, 
                         packet.data.midi1.channel,
                         packet.data.midi1.data[0],
                         packet.data.midi1.data[1]);
                
                // Option 1: Loop back to UART TX (echo)
                // if (router_state.uart_loopback) {
                //     ret = midi_uart_send_message(&packet.data.midi1);
                //     if (ret != ESP_OK) {
                //         ESP_LOGW(TAG, "Failed to send UART echo: %s", 
                //                  esp_err_to_name(ret));
                //     }
                // }
                
                // Option 2: Send to USB (MIDI 1.0 format)
                if (router_state.enable_usb) {
                    ret = midi_usbd_send(packet.cable, &packet.data.midi1);
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to send to USB: %s", 
                                 esp_err_to_name(ret));
                    } else {
                        ESP_LOGD(TAG, "UART→USB sent successfully");
                    }
                }
                
                // Option 3: Translate to MIDI 2.0 and send to network
                if (router_state.enable_network) {
                    ret = midi_translate_1to2(&packet.data.midi1, &translated_ump);
                    if (ret == ESP_OK) {
                        ret = midi_net_broadcast_ump(&midi_server, &translated_ump);
                        if (ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to send to network: %s", 
                                     esp_err_to_name(ret));
                        } else {
                            ESP_LOGD(TAG, "UART→Network sent successfully");
                        }
                    } else {
                        ESP_LOGW(TAG, "UART message translation failed: %s", 
                                 esp_err_to_name(ret));
                    }
                }
                break;
            }
            
            // ============================================================
            // USB MIDI Input - Route to UART and Network
            // ============================================================
            case MIDI_TRANSPORT_USB: {
                ESP_LOGD(TAG, "USB→Router: Status=0x%02X Ch=%d Data=[0x%02X 0x%02X]", 
                         packet.data.midi1.status,
                         packet.data.midi1.channel,
                         packet.data.midi1.data[0],
                         packet.data.midi1.data[1]);
                
                // Option 1: Send to UART
                if (router_state.enable_uart) {
                    ret = midi_uart_send_message(&packet.data.midi1);
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to send to UART: %s", 
                                 esp_err_to_name(ret));
                    } else {
                        ESP_LOGD(TAG, "USB→UART sent successfully");
                    }
                }
                
                // Option 2: Translate to MIDI 2.0 and send to network
                if (router_state.enable_network) {
                    ret = midi_translate_1to2(&packet.data.midi1, &translated_ump);
                    if (ret == ESP_OK) {
                        ret = midi_net_broadcast_ump(&midi_server, &translated_ump);
                        if (ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to send to network: %s", 
                                     esp_err_to_name(ret));
                        } else {
                            ESP_LOGD(TAG, "USB→Network sent successfully");
                        }
                    } else {
                        ESP_LOGW(TAG, "USB message translation failed: %s", 
                                 esp_err_to_name(ret));
                    }
                }
                break;
            }
            
            // ============================================================
            // Network MIDI Input (UMP/MIDI 2.0) - Route to UART and USB
            // ============================================================
            case MIDI_TRANSPORT_NETWORK: {
                uint8_t mt = (packet.data.ump.words[0] >> 28) & 0x0F;
                ESP_LOGD(TAG, "Network→Router: MT=0x%X Word0=0x%08lX Word1=0x%08lX", 
                         mt, packet.data.ump.words[0], packet.data.ump.words[1]);
                
                // Translate MIDI 2.0 to MIDI 1.0
                ret = midi_translate_2to1(&packet.data.ump, &translated_msg);
                
                if (ret == ESP_OK) {
                    // Option 1: Send to UART
                    if (router_state.enable_uart) {
                        ret = midi_uart_send_message(&translated_msg);
                        if (ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to send to UART: %s", 
                                     esp_err_to_name(ret));
                        } else {
                            ESP_LOGD(TAG, "Network→UART sent successfully");
                        }
                    }
                    
                    // Option 2: Send to USB
                    if (router_state.enable_usb) {
                        ret = midi_usbd_send(packet.cable, &translated_msg);
                        if (ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to send to USB: %s", 
                                     esp_err_to_name(ret));
                        } else {
                            ESP_LOGD(TAG, "Network→USB sent successfully");
                        }
                    }
                } else if (ret == ESP_ERR_NOT_SUPPORTED) {
                    ESP_LOGW(TAG, "Network message cannot be translated to MIDI 1.0");
                } else {
                    ESP_LOGE(TAG, "Network message translation failed: %s", 
                             esp_err_to_name(ret));
                }
                break;
            }
            
            default:
                ESP_LOGW(TAG, "Unknown packet source: %d", packet.source);
                break;
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

    router_state.enable_uart = true;
    router_state.enable_usb = true;
    router_state.enable_network = true;
    router_state.uart_loopback = true;
    
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

