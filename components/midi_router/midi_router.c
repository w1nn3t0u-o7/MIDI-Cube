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
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "midi_router";

#define ROUTER_QUEUE_SIZE 64
#define ROUTER_TASK_STACK_SIZE 4096
#define ROUTER_TASK_PRIORITY 10
#define ROUTER_TASK_CORE 1

/**
 * @brief Router state
 */
typedef struct {
    bool initialized;
    midi_router_config_t config;
    midi_router_stats_t stats;
    
    // Message queue
    QueueHandle_t packet_queue;
    
    // Router task
    TaskHandle_t router_task_handle;
    
    // Transport callbacks (registered by transport layers)
    esp_err_t (*transport_tx_callbacks[MIDI_TRANSPORT_COUNT])(const midi_packet_t *);
    
} midi_router_state_t;

static midi_router_state_t g_router_state = {0};

// Transport name strings
static const char *transport_names[] = {
    "UART", "USB", "Ethernet", "WiFi"
};

/**
 * @brief Check if message passes filter
 */
static bool midi_router_check_filter(const midi_packet_t *packet, 
                                      const midi_filter_t *filter) {
    if (!filter->enabled) {
        return true;  // Filter disabled, pass all
    }
    
    // Extract channel (if applicable)
    uint8_t channel = 0;
    uint8_t status = 0;
    
    if (packet->format == 0) {  // MIDI 1.0
        status = packet->data.midi1.status;
        channel = packet->data.midi1.channel;
        
        // Check channel filter
        if (status < 0xF0) {  // Channel message
            if (!(filter->channel_mask & (1 << channel))) {
                return false;  // Channel blocked
            }
        }
        
        // Check specific filters
        if (filter->block_active_sensing && status == 0xFE) {
            return false;
        }
        if (filter->block_clock && status == 0xF8) {
            return false;
        }
    }
    
    return true;  // Passed all filters
}

/**
 * @brief Translate packet if needed
 */
static esp_err_t midi_router_translate(midi_packet_t *packet, 
                                        bool dest_wants_ump) {
    if (!g_router_state.config.auto_translate) {
        return ESP_OK;  // Translation disabled
    }
    
    bool src_is_midi1 = (packet->format == 0);
    
    if (src_is_midi1 && dest_wants_ump) {
        // MIDI 1.0 → UMP
        ump_packet_t ump;
        esp_err_t err = midi_translate_1to2(&packet->data.midi1, &ump);
        if (err == ESP_OK) {
            packet->format = 1;
            packet->data.ump = ump;
            g_router_state.stats.translations_1to2++;
        }
        return err;
    } else if (!src_is_midi1 && !dest_wants_ump) {
        // UMP → MIDI 1.0
        midi_message_t midi1;
        esp_err_t err = midi_translate_2to1(&packet->data.ump, &midi1);
        if (err == ESP_OK) {
            packet->format = 0;
            packet->data.midi1 = midi1;
            g_router_state.stats.translations_2to1++;
        }
        return err;
    }
    
    return ESP_OK;  // No translation needed
}

/**
 * @brief Router task - processes incoming packets
 */
static void midi_router_task(void *arg) {
    midi_packet_t packet;
    
    ESP_LOGI(TAG, "Router task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Wait for packet
        if (xQueueReceive(g_router_state.packet_queue, &packet, 
                         portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        midi_transport_t src = packet.source;
        
        // Apply input filter
        if (!midi_router_check_filter(&packet, 
                                      &g_router_state.config.input_filters[src])) {
            g_router_state.stats.packets_filtered[src]++;
            continue;  // Filtered out
        }
        
        // Determine destinations
        bool merge_mode = g_router_state.config.merge_inputs;
        
        for (int dest = 0; dest < MIDI_TRANSPORT_COUNT; dest++) {
            // Check if route enabled
            bool route_enabled = merge_mode || 
                                g_router_state.config.routing_matrix[src][dest];
            
            if (!route_enabled) {
                continue;  // Route blocked
            }
            
            // Don't route back to source (avoid loops)
            if (dest == src) {
                continue;
            }
            
            // Translate if destination requires different format
            midi_packet_t out_packet = packet;
            bool dest_wants_ump = (dest == MIDI_TRANSPORT_ETHERNET || 
                                   dest == MIDI_TRANSPORT_WIFI ||
                                   dest == MIDI_TRANSPORT_USB);  // USB can do both
            
            esp_err_t err = midi_router_translate(&out_packet, dest_wants_ump);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Translation failed: %s → %s",
                         transport_names[src], transport_names[dest]);
                g_router_state.stats.routing_errors++;
                continue;
            }
            
            // Send to transport TX callback
            if (g_router_state.transport_tx_callbacks[dest]) {
                err = g_router_state.transport_tx_callbacks[dest](&out_packet);
                if (err == ESP_OK) {
                    g_router_state.stats.packets_routed[src][dest]++;
                } else {
                    g_router_state.stats.packets_dropped[dest]++;
                    ESP_LOGW(TAG, "TX failed: %s", transport_names[dest]);
                }
            } else {
                ESP_LOGD(TAG, "No TX callback for %s", transport_names[dest]);
            }
        }
    }
}

/**
 * @brief Initialize router
 */
esp_err_t midi_router_init(const midi_router_config_t *config) {
    if (g_router_state.initialized) {
        ESP_LOGW(TAG, "Router already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing MIDI router");
    
    // Clear state
    memset(&g_router_state, 0, sizeof(g_router_state));
    
    // Load or use provided config
    if (config) {
        g_router_state.config = *config;
    } else {
        esp_err_t err = midi_router_load_config();
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "No saved config, using defaults");
            midi_router_reset_config();
        }
    }
    
    // Create packet queue
    g_router_state.packet_queue = xQueueCreate(ROUTER_QUEUE_SIZE, 
                                                sizeof(midi_packet_t));
    if (!g_router_state.packet_queue) {
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
        &g_router_state.router_task_handle,
        ROUTER_TASK_CORE
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create router task");
        vQueueDelete(g_router_state.packet_queue);
        return ESP_FAIL;
    }
    
    g_router_state.initialized = true;
    ESP_LOGI(TAG, "MIDI router initialized");
    
    // Print routing matrix
    ESP_LOGI(TAG, "Routing matrix:");
    for (int src = 0; src < MIDI_TRANSPORT_COUNT; src++) {
        ESP_LOGI(TAG, "  %s →", transport_names[src]);
        for (int dest = 0; dest < MIDI_TRANSPORT_COUNT; dest++) {
            if (g_router_state.config.routing_matrix[src][dest]) {
                ESP_LOGI(TAG, "    ✓ %s", transport_names[dest]);
            }
        }
    }
    
    return ESP_OK;
}

/**
 * @brief Send packet to router
 */
esp_err_t midi_router_send(const midi_packet_t *packet) {
    if (!g_router_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xQueueSend(g_router_state.packet_queue, packet, 0) != pdTRUE) {
        g_router_state.stats.packets_dropped[packet->source]++;
        return ESP_ERR_NO_MEM;  // Queue full
    }
    
    return ESP_OK;
}

/**
 * @brief Register transport TX callback
 */
esp_err_t midi_router_register_transport_tx(midi_transport_t transport,
                                             esp_err_t (*tx_callback)(const midi_packet_t *)) {
    if (transport >= MIDI_TRANSPORT_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_router_state.transport_tx_callbacks[transport] = tx_callback;
    ESP_LOGI(TAG, "Registered TX callback for %s", transport_names[transport]);
    
    return ESP_OK;
}

// ... (additional functions: set_route, get_route, save_config, etc.)
// [Implementation continues with NVS operations, config management]

/**
 * @brief Get transport name
 */
const char* midi_router_get_transport_name(midi_transport_t transport) {
    if (transport < MIDI_TRANSPORT_COUNT) {
        return transport_names[transport];
    }
    return "Unknown";
}

