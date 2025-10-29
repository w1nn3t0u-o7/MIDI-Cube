/**
 * @file midi_wifi.c
 * @brief MIDI 2.0 over WiFi - Main Implementation
 * 
 * Network MIDI 2.0 using UDP transport
 */

#include "midi_wifi.h"
#include "midi_wifi_session.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "midi_wifi";

// WiFi event bits
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAIL_BIT         BIT1

// Network MIDI 2.0 constants[file:4]
#define MIDI_WIFI_DEFAULT_PORT        5004
#define MIDI_WIFI_MTU                 1472  // Max UDP payload to fit in single packet
#define MIDI_WIFI_SERVICE_NAME        "_midi2._udp"
#define MIDI_WIFI_KEEPALIVE_INTERVAL  1000  // 1 second
#define MIDI_WIFI_SESSION_TIMEOUT     5000  // 5 seconds

/**
 * @brief WiFi MIDI driver state
 */
typedef struct {
    bool initialized;
    bool wifi_connected;
    midi_wifi_config_t config;
    midi_wifi_stats_t stats;
    
    // WiFi
    esp_netif_t *netif;
    EventGroupHandle_t wifi_event_group;
    int wifi_retry_num;
    
    // UDP socket
    int sock_fd;
    struct sockaddr_in local_addr;
    
    // Tasks
    TaskHandle_t rx_task_handle;
    TaskHandle_t keepalive_task_handle;
    
    // Session management
    midi_wifi_peer_t peers[CONFIG_MIDI_WIFI_MAX_CLIENTS];
    uint8_t num_active_peers;
    SemaphoreHandle_t peers_mutex;
    
    // Discovery (managed by midi_wifi_discovery.c)
    midi_wifi_discovered_device_t discovered[16];
    uint8_t num_discovered;
    SemaphoreHandle_t discovery_mutex;
    
    // FEC buffer (for Forward Error Correction)
    ump_packet_t *fec_buffer;
    uint16_t fec_buffer_size;
    uint16_t fec_buffer_idx;
    
    // Retransmit buffer
    struct {
        ump_packet_t packet;
        uint32_t sequence_num;
        uint32_t timestamp_ms;
    } *retransmit_buffer;
    uint16_t retransmit_buffer_size;
    uint16_t retransmit_buffer_idx;
    uint32_t tx_sequence_num;
    
} midi_wifi_state_t;

static midi_wifi_state_t g_wifi_state = {0};

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (g_wifi_state.wifi_retry_num < 10) {
            esp_wifi_connect();
            g_wifi_state.wifi_retry_num++;
            ESP_LOGI(TAG, "Retry WiFi connection (%d/10)", g_wifi_state.wifi_retry_num);
        } else {
            xEventGroupSetBits(g_wifi_state.wifi_event_group, WIFI_FAIL_BIT);
        }
        g_wifi_state.wifi_connected = false;
        ESP_LOGI(TAG, "WiFi disconnected");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_state.wifi_retry_num = 0;
        g_wifi_state.wifi_connected = true;
        xEventGroupSetBits(g_wifi_state.wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize WiFi station mode
 */
static esp_err_t wifi_init_sta(void) {
    g_wifi_state.wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    g_wifi_state.netif = esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi station initialized");
    
    return ESP_OK;
}

/**
 * @brief Initialize UDP socket
 */
static esp_err_t udp_socket_init(void) {
    g_wifi_state.sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_wifi_state.sock_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(g_wifi_state.sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set receive timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(g_wifi_state.sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Bind to local port
    g_wifi_state.local_addr.sin_family = AF_INET;
    g_wifi_state.local_addr.sin_addr.s_addr = INADDR_ANY;
    g_wifi_state.local_addr.sin_port = htons(g_wifi_state.config.host_port);
    
    int err = bind(g_wifi_state.sock_fd, 
                   (struct sockaddr *)&g_wifi_state.local_addr,
                   sizeof(g_wifi_state.local_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(g_wifi_state.sock_fd);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "UDP socket bound to port %d", g_wifi_state.config.host_port);
    
    return ESP_OK;
}

/**
 * @brief UDP RX task - receives packets from network
 */
static void midi_wifi_rx_task(void *arg) {
    uint8_t rx_buffer[MIDI_WIFI_MTU];
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    
    ESP_LOGI(TAG, "WiFi MIDI RX task started");
    
    while (1) {
        // Receive UDP packet
        int len = recvfrom(g_wifi_state.sock_fd, rx_buffer, sizeof(rx_buffer), 0,
                          (struct sockaddr *)&src_addr, &src_addr_len);
        
        if (len > 0) {
            // Convert source address to string
            char src_ip[16];
            inet_ntoa_r(src_addr.sin_addr, src_ip, sizeof(src_ip));
            uint16_t src_port = ntohs(src_addr.sin_port);
            
            ESP_LOGD(TAG, "RX: %d bytes from %s:%d", len, src_ip, src_port);
            
            g_wifi_state.stats.packets_rx_total++;
            
            // Handle packet via session manager
            midi_wifi_session_handle_packet(rx_buffer, len, src_ip, src_port);
            
        } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGW(TAG, "recvfrom failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/**
 * @brief Keepalive task - sends periodic keepalive packets
 */
static void midi_wifi_keepalive_task(void *arg) {
    ESP_LOGI(TAG, "Keepalive task started");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MIDI_WIFI_KEEPALIVE_INTERVAL));
        
        if (g_wifi_state.wifi_connected && g_wifi_state.num_active_peers > 0) {
            midi_wifi_session_send_keepalive();
        }
    }
}

/**
 * @brief Initialize mDNS for service discovery[file:4]
 */
static esp_err_t mdns_init_service(void) {
    if (!g_wifi_state.config.enable_mdns) {
        return ESP_OK;
    }
    
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Set hostname
    char hostname[32];
    snprintf(hostname, sizeof(hostname), "midi-cube-%02x%02x", 
             esp_random() & 0xFF, (esp_random() >> 8) & 0xFF);
    mdns_hostname_set(hostname);
    
    // Set instance name
    mdns_instance_name_set(g_wifi_state.config.endpoint_name);
    
    // Add MIDI 2.0 service[file:4]
    err = mdns_service_add(NULL, MIDI_WIFI_SERVICE_NAME, "_udp", 
                           g_wifi_state.config.host_port, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS service add failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Add service TXT records[file:4]
    mdns_service_txt_item_t txt_data[] = {
        {"name", g_wifi_state.config.endpoint_name},
        {"fec", g_wifi_state.config.enable_fec ? "1" : "0"},
        {"retx", g_wifi_state.config.enable_retransmit ? "1" : "0"}
    };
    
    mdns_service_txt_set(MIDI_WIFI_SERVICE_NAME, "_udp", txt_data, 3);
    
    ESP_LOGI(TAG, "mDNS service registered: %s.%s.local:%d",
             hostname, MIDI_WIFI_SERVICE_NAME, g_wifi_state.config.host_port);
    
    return ESP_OK;
}

/**
 * @brief Initialize MIDI WiFi driver
 */
esp_err_t midi_wifi_init(const midi_wifi_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_wifi_state.initialized) {
        ESP_LOGW(TAG, "WiFi MIDI already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing MIDI WiFi");
    
    // Clear state
    memset(&g_wifi_state, 0, sizeof(g_wifi_state));
    g_wifi_state.config = *config;
    
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    // Create mutexes
    g_wifi_state.peers_mutex = xSemaphoreCreateMutex();
    g_wifi_state.discovery_mutex = xSemaphoreCreateMutex();
    
    if (!g_wifi_state.peers_mutex || !g_wifi_state.discovery_mutex) {
        ESP_LOGE(TAG, "Failed to create mutexes");
        return ESP_FAIL;
    }
    
    // Allocate FEC buffer if enabled
    if (config->enable_fec) {
        g_wifi_state.fec_buffer = malloc(sizeof(ump_packet_t) * 2);
        g_wifi_state.fec_buffer_size = 2;
    }
    
    // Allocate retransmit buffer if enabled
    if (config->enable_retransmit) {
        g_wifi_state.retransmit_buffer = calloc(config->retransmit_buffer_size,
                                                 sizeof(*g_wifi_state.retransmit_buffer));
        g_wifi_state.retransmit_buffer_size = config->retransmit_buffer_size;
    }
    
    // Initialize WiFi
    err = wifi_init_sta();
    if (err != ESP_OK) {
        return err;
    }
    
    // Initialize session manager
    err = midi_wifi_session_init(config);
    if (err != ESP_OK) {
        return err;
    }
    
    g_wifi_state.initialized = true;
    
    ESP_LOGI(TAG, "MIDI WiFi initialized (mode: %s)",
             config->mode == MIDI_WIFI_MODE_HOST ? "HOST" :
             config->mode == MIDI_WIFI_MODE_CLIENT ? "CLIENT" : "BOTH");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize MIDI WiFi driver
 */
esp_err_t midi_wifi_deinit(void) {
    if (!g_wifi_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing MIDI WiFi");
    
    // Stop tasks
    if (g_wifi_state.rx_task_handle) {
        vTaskDelete(g_wifi_state.rx_task_handle);
    }
    if (g_wifi_state.keepalive_task_handle) {
        vTaskDelete(g_wifi_state.keepalive_task_handle);
    }
    
    // Close socket
    if (g_wifi_state.sock_fd >= 0) {
        close(g_wifi_state.sock_fd);
    }
    
    // Stop mDNS
    if (g_wifi_state.config.enable_mdns) {
        mdns_free();
    }
    
    // Deinitialize session manager
    midi_wifi_session_deinit();
    
    // Stop WiFi
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Free buffers
    if (g_wifi_state.fec_buffer) {
        free(g_wifi_state.fec_buffer);
    }
    if (g_wifi_state.retransmit_buffer) {
        free(g_wifi_state.retransmit_buffer);
    }
    
    // Delete mutexes
    if (g_wifi_state.peers_mutex) {
        vSemaphoreDelete(g_wifi_state.peers_mutex);
    }
    if (g_wifi_state.discovery_mutex) {
        vSemaphoreDelete(g_wifi_state.discovery_mutex);
    }
    
    g_wifi_state.initialized = false;
    
    return ESP_OK;
}

/**
 * @brief Connect to WiFi AP
 */
esp_err_t midi_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms) {
    if (!g_wifi_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);
    
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(g_wifi_state.wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            pdMS_TO_TICKS(timeout_ms ? timeout_ms : portMAX_DELAY));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi");
        
        // Initialize UDP socket
        esp_err_t err = udp_socket_init();
        if (err != ESP_OK) {
            return err;
        }
        
        // Initialize mDNS
        err = mdns_init_service();
        if (err != ESP_OK) {
            return err;
        }
        
        // Create RX task
        xTaskCreate(midi_wifi_rx_task, "midi_wifi_rx", 4096, NULL, 10, &g_wifi_state.rx_task_handle);
        
        // Create keepalive task
        xTaskCreate(midi_wifi_keepalive_task, "midi_wifi_ka", 2048, NULL, 5, &g_wifi_state.keepalive_task_handle);
        
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    }
}

/**
 * @brief Send UMP to all connected peers
 */
esp_err_t midi_wifi_send_ump(const ump_packet_t *ump) {
    if (!g_wifi_state.initialized || !g_wifi_state.wifi_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ump) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build packet payload[file:4]
    uint8_t payload[MIDI_WIFI_MTU];
    size_t payload_len = 0;
    
    // Packet header: type (1 byte) + sequence (4 bytes)
    payload[0] = MIDI_WIFI_PKT_UMP;
    uint32_t seq = g_wifi_state.tx_sequence_num++;
    memcpy(&payload[1], &seq, 4);
    payload_len = 5;
    
    // Add UMP words
    uint32_t ump_bytes = ump->num_words * 4;
    memcpy(&payload[payload_len], ump->words, ump_bytes);
    payload_len += ump_bytes;
    
    // Send to all active peers
    xSemaphoreTake(g_wifi_state.peers_mutex, portMAX_DELAY);
    
    for (int i = 0; i < g_wifi_state.num_active_peers; i++) {
        midi_wifi_peer_t *peer = &g_wifi_state.peers[i];
        
        if (peer->state != MIDI_WIFI_SESSION_CONNECTED) {
            continue;
        }
        
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(peer->port);
        inet_pton(AF_INET, peer->ip_addr, &dest_addr.sin_addr);
        
        int sent = sendto(g_wifi_state.sock_fd, payload, payload_len, 0,
                         (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        if (sent == payload_len) {
            peer->packets_tx++;
            g_wifi_state.stats.packets_tx_total++;
        } else {
            ESP_LOGW(TAG, "Failed to send to %s:%d", peer->ip_addr, peer->port);
        }
    }
    
    xSemaphoreGive(g_wifi_state.peers_mutex);
    
    // Store in retransmit buffer if enabled
    if (g_wifi_state.retransmit_buffer) {
        uint16_t idx = g_wifi_state.retransmit_buffer_idx++ % g_wifi_state.retransmit_buffer_size;
        g_wifi_state.retransmit_buffer[idx].packet = *ump;
        g_wifi_state.retransmit_buffer[idx].sequence_num = seq;
        g_wifi_state.retransmit_buffer[idx].timestamp_ms = esp_timer_get_time() / 1000;
    }
    
    return ESP_OK;
}

// ... (remaining helper functions: get_stats, get_peers, etc.)

/**
 * @brief Check if WiFi is connected
 */
bool midi_wifi_is_connected(void) {
    return g_wifi_state.wifi_connected;
}

/**
 * @brief Get WiFi MIDI statistics
 */
esp_err_t midi_wifi_get_stats(midi_wifi_stats_t *stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *stats = g_wifi_state.stats;
    stats->active_sessions = g_wifi_state.num_active_peers;
    
    return ESP_OK;
}
