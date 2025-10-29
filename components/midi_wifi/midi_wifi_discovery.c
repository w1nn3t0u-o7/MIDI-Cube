/**
 * @file midi_wifi_discovery.c
 * @brief MIDI WiFi Device Discovery via mDNS
 * 
 * Discovers MIDI 2.0 devices on network using DNS-SD
 */

#include "midi_wifi.h"
#include "esp_log.h"
#include "mdns.h"
#include <string.h>

static const char *TAG = "wifi_discovery";

// External access to main state
extern midi_wifi_state_t g_wifi_state;

/**
 * @brief Query mDNS for MIDI 2.0 services[file:4]
 */
esp_err_t midi_wifi_start_discovery(uint32_t scan_duration_ms) {
    if (!g_wifi_state.config.enable_mdns) {
        ESP_LOGW(TAG, "mDNS discovery disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    ESP_LOGI(TAG, "Starting MIDI device discovery (service: %s)", MIDI_WIFI_SERVICE_NAME);
    
    // Query for _midi2._udp services
    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(MIDI_WIFI_SERVICE_NAME, "_udp", 
                                    scan_duration_ms ? scan_duration_ms : 3000,
                                    20, &results);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS query failed: %s", esp_err_to_name(err));
        return err;
    }
    
    xSemaphoreTake(g_wifi_state.discovery_mutex, portMAX_DELAY);
    
    g_wifi_state.num_discovered = 0;
    
    // Process results
    mdns_result_t *r = results;
    while (r && g_wifi_state.num_discovered < 16) {
        midi_wifi_discovered_device_t *dev = &g_wifi_state.discovered[g_wifi_state.num_discovered];
        
        // Get IP address
        mdns_ip_addr_t *addr = r->addr;
        if (addr) {
            sprintf(dev->ip_addr, IPSTR, IP2STR(&addr->addr.u_addr.ip4));
        }
        
        // Get port
        dev->port = r->port;
        
        // Get instance name
        if (r->instance_name) {
            strncpy(dev->instance_name, r->instance_name, sizeof(dev->instance_name) - 1);
        }
        
        // Get hostname as endpoint name
        if (r->hostname) {
            strncpy(dev->endpoint_name, r->hostname, sizeof(dev->endpoint_name) - 1);
        }
        
        // Parse TXT records for capabilities
        mdns_txt_item_t *txt = r->txt;
        while (txt) {
            if (strcmp(txt->key, "fec") == 0) {
                dev->supports_fec = (txt->value[0] == '1');
            } else if (strcmp(txt->key, "retx") == 0) {
                dev->supports_retransmit = (txt->value[0] == '1');
            }
            txt = txt->next;
        }
        
        ESP_LOGI(TAG, "Discovered: %s at %s:%d (FEC:%d, Retx:%d)",
                 dev->endpoint_name, dev->ip_addr, dev->port,
                 dev->supports_fec, dev->supports_retransmit);
        
        // Notify application
        if (g_wifi_state.config.discovery_callback) {
            g_wifi_state.config.discovery_callback(dev, g_wifi_state.config.callback_ctx);
        }
        
        g_wifi_state.num_discovered++;
        r = r->next;
    }
    
    g_wifi_state.stats.discovery_count = g_wifi_state.num_discovered;
    
    xSemaphoreGive(g_wifi_state.discovery_mutex);
    
    // Free results
    mdns_query_results_free(results);
    
    ESP_LOGI(TAG, "Discovery complete: %d devices found", g_wifi_state.num_discovered);
    
    return ESP_OK;
}

/**
 * @brief Stop discovery scan
 */
esp_err_t midi_wifi_stop_discovery(void) {
    // mDNS query is blocking, so nothing to stop
    return ESP_OK;
}

/**
 * @brief Get list of discovered devices
 */
esp_err_t midi_wifi_get_discovered_devices(midi_wifi_discovered_device_t *devices,
                                            uint8_t max_devices,
                                            uint8_t *num_devices) {
    if (!devices || !num_devices) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_wifi_state.discovery_mutex, portMAX_DELAY);
    
    uint8_t count = (g_wifi_state.num_discovered < max_devices) ? 
                     g_wifi_state.num_discovered : max_devices;
    
    memcpy(devices, g_wifi_state.discovered, count * sizeof(midi_wifi_discovered_device_t));
    *num_devices = count;
    
    xSemaphoreGive(g_wifi_state.discovery_mutex);
    
    return ESP_OK;
}
