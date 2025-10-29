/**
 * @file midi_ethernet.c
 * @brief MIDI 2.0 over Ethernet - Main Implementation
 * 
 * Uses W5500 for Network MIDI 2.0 via UDP
 */

#include "midi_ethernet.h"
#include "midi_ethernet_session.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mdns.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "midi_eth";

// Event bits
#define ETH_LINK_UP_BIT      BIT0
#define ETH_GOT_IP_BIT       BIT1

// Network MIDI 2.0 constants (same as WiFi)
#define MIDI_ETH_DEFAULT_PORT  5004
#define MIDI_ETH_MTU           1472
#define MIDI_ETH_SERVICE_NAME  "_midi2._udp"

// Forward declarations
extern esp_err_t midi_ethernet_w5500_init_spi(const midi_ethernet_config_t *config,
                                               spi_device_handle_t *spi_handle);
extern esp_err_t midi_ethernet_w5500_init_driver(const midi_ethernet_config_t *config,
                                                  esp_eth_handle_t *eth_handle,
                                                  esp_netif_t **netif);
extern esp_err_t midi_ethernet_w5500_configure_ip(esp_netif_t *netif,
                                                   const midi_ethernet_config_t *config);

/**
 * @brief Ethernet MIDI driver state
 */
typedef struct {
    bool initialized;
    bool link_up;
    bool ip_assigned;
    midi_ethernet_config_t config;
    midi_ethernet_stats_t stats;
    
    // Hardware
    spi_device_handle_t spi_handle;
    esp_eth_handle_t eth_handle;
    esp_netif_t *netif;
    EventGroupHandle_t eth_event_group;
    
    // UDP socket
    int sock_fd;
    struct sockaddr_in local_addr;
    
    // Tasks
    TaskHandle_t rx_task_handle;
    TaskHandle_t keepalive_task_handle;
    
    // Session management (reuse WiFi session code)
    midi_ethernet_peer_t peers[CONFIG_MIDI_ETH_MAX_CLIENTS];
    uint8_t num_active_peers;
    SemaphoreHandle_t peers_mutex;
    
    // Sequence number
    uint32_t tx_sequence_num;
    
} midi_ethernet_state_t;

static midi_ethernet_state_t g_eth_state = {0};

/**
 * @brief Ethernet event handler[web:276][web:277]
 */
static void eth_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == ETH_EVENT) {
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Ethernet link up");
                g_eth_state.link_up = true;
                g_eth_state.stats.link_up = true;
                xEventGroupSetBits(g_eth_state.eth_event_group, ETH_LINK_UP_BIT);
                break;
                
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "Ethernet link down");
                g_eth_state.link_up = false;
                g_eth_state.stats.link_up = false;
                xEventGroupClearBits(g_eth_state.eth_event_group, ETH_LINK_UP_BIT);
                break;
                
            case ETHERNET_EVENT_START:
                ESP_LOGI(TAG, "Ethernet started");
                break;
                
            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG, "Ethernet stopped");
                break;
                
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_ETH_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
            
            g_eth_state.ip_assigned = true;
            g_eth_state.stats.ip_assigned = true;
            xEventGroupSetBits(g_eth_state.eth_event_group, ETH_GOT_IP_BIT);
        }
    }
}

/**
 * @brief Initialize UDP socket
 */
static esp_err_t udp_socket_init(void) {
    g_eth_state.sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_eth_state.sock_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(g_eth_state.sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set receive timeout
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(g_eth_state.sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Bind to local port
    g_eth_state.local_addr.sin_family = AF_INET;
    g_eth_state.local_addr.sin_addr.s_addr = INADDR_ANY;
    g_eth_state.local_addr.sin_port = htons(g_eth_state.config.host_port);
    
    int err = bind(g_eth_state.sock_fd,
                   (struct sockaddr *)&g_eth_state.local_addr,
                   sizeof(g_eth_state.local_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(g_eth_state.sock_fd);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "UDP socket bound to port %d", g_eth_state.config.host_port);
    return ESP_OK;
}

/**
 * @brief UDP RX task
 */
static void midi_ethernet_rx_task(void *arg) {
    uint8_t rx_buffer[MIDI_ETH_MTU];
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    
    ESP_LOGI(TAG, "Ethernet MIDI RX task started");
    
    while (1) {
        int len = recvfrom(g_eth_state.sock_fd, rx_buffer, sizeof(rx_buffer), 0,
                          (struct sockaddr *)&src_addr, &src_addr_len);
        
        if (len > 0) {
            char src_ip[16];
            inet_ntoa_r(src_addr.sin_addr, src_ip, sizeof(src_ip));
            uint16_t src_port = ntohs(src_addr.sin_port);
            
            g_eth_state.stats.packets_rx_total++;
            
            // Handle via session manager (same as WiFi)
            midi_ethernet_session_handle_packet(rx_buffer, len, src_ip, src_port);
            
        } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGW(TAG, "recvfrom failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/**
 * @brief Keepalive task
 */
static void midi_ethernet_keepalive_task(void *arg) {
    ESP_LOGI(TAG, "Keepalive task started");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (g_eth_state.link_up && g_eth_state.num_active_peers > 0) {
            midi_ethernet_session_send_keepalive();
        }
    }
}

/**
 * @brief Initialize mDNS service[file:4]
 */
static esp_err_t mdns_init_service(void) {
    if (!g_eth_state.config.enable_mdns) {
        return ESP_OK;
    }
    
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }
    
    char hostname[32];
    uint8_t mac[6];
    esp_netif_get_mac(g_eth_state.netif, mac);
    snprintf(hostname, sizeof(hostname), "midi-eth-%02x%02x", mac[4], mac[5]);
    
    mdns_hostname_set(hostname);
    mdns_instance_name_set(g_eth_state.config.endpoint_name);
    
    err = mdns_service_add(NULL, MIDI_ETH_SERVICE_NAME, "_udp",
                           g_eth_state.config.host_port, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS service add failed: %s", esp_err_to_name(err));
        return err;
    }
    
    mdns_service_txt_item_t txt_data[] = {
        {"name", g_eth_state.config.endpoint_name},
        {"fec", g_eth_state.config.enable_fec ? "1" : "0"},
        {"retx", g_eth_state.config.enable_retransmit ? "1" : "0"}
    };
    mdns_service_txt_set(MIDI_ETH_SERVICE_NAME, "_udp", txt_data, 3);
    
    ESP_LOGI(TAG, "mDNS service registered: %s.%s.local:%d",
             hostname, MIDI_ETH_SERVICE_NAME, g_eth_state.config.host_port);
    
    return ESP_OK;
}

/**
 * @brief Initialize Ethernet MIDI driver
 */
esp_err_t midi_ethernet_init(const midi_ethernet_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_eth_state.initialized) {
        ESP_LOGW(TAG, "Ethernet MIDI already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing MIDI Ethernet (W5500)");
    
    memset(&g_eth_state, 0, sizeof(g_eth_state));
    g_eth_state.config = *config;
    
    // Create event group
    g_eth_state.eth_event_group = xEventGroupCreate();
    
    // Create mutexes
    g_eth_state.peers_mutex = xSemaphoreCreateMutex();
    
    // Initialize ESP-IDF networking
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Register event handlers[web:276][web:277]
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                &eth_event_handler, NULL));
    
    // Initialize W5500 SPI
    esp_err_t err = midi_ethernet_w5500_init_spi(config, &g_eth_state.spi_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    // Initialize W5500 driver[web:279][web:281]
    err = midi_ethernet_w5500_init_driver(config, &g_eth_state.eth_handle,
                                          &g_eth_state.netif);
    if (err != ESP_OK) {
        return err;
    }
    
    // Configure IP (DHCP or static)
    err = midi_ethernet_w5500_configure_ip(g_eth_state.netif, config);
    if (err != ESP_OK) {
        return err;
    }
    
    // Start Ethernet driver[web:276]
    err = esp_eth_start(g_eth_state.eth_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet start failed: %s", esp_err_to_name(err));
        return err;
    }
    
    g_eth_state.initialized = true;
    
    ESP_LOGI(TAG, "MIDI Ethernet initialized successfully");
    ESP_LOGI(TAG, "Waiting for link up and IP address...");
    
    return ESP_OK;
}

/**
 * @brief Wait for Ethernet link up
 */
esp_err_t midi_ethernet_wait_for_link(uint32_t timeout_ms) {
    EventBits_t bits = xEventGroupWaitBits(g_eth_state.eth_event_group,
                                            ETH_LINK_UP_BIT | ETH_GOT_IP_BIT,
                                            pdFALSE, pdTRUE,
                                            pdMS_TO_TICKS(timeout_ms ? timeout_ms : portMAX_DELAY));
    
    if ((bits & ETH_LINK_UP_BIT) && (bits & ETH_GOT_IP_BIT)) {
        ESP_LOGI(TAG, "Ethernet ready!");
        
        // Initialize UDP socket
        esp_err_t err = udp_socket_init();
        if (err != ESP_OK) return err;
        
        // Initialize mDNS
        err = mdns_init_service();
        if (err != ESP_OK) return err;
        
        // Create tasks
        xTaskCreate(midi_ethernet_rx_task, "midi_eth_rx", 4096, NULL, 10,
                   &g_eth_state.rx_task_handle);
        xTaskCreate(midi_ethernet_keepalive_task, "midi_eth_ka", 2048, NULL, 5,
                   &g_eth_state.keepalive_task_handle);
        
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Send UMP to all connected peers[file:4]
 */
esp_err_t midi_ethernet_send_ump(const ump_packet_t *ump) {
    if (!g_eth_state.initialized || !g_eth_state.link_up) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build packet (same format as WiFi)
    uint8_t payload[MIDI_ETH_MTU];
    payload[0] = 0x00;  // PKT_UMP
    uint32_t seq = g_eth_state.tx_sequence_num++;
    memcpy(&payload[1], &seq, 4);
    
    uint32_t ump_bytes = ump->num_words * 4;
    memcpy(&payload[5], ump->words, ump_bytes);
    size_t payload_len = 5 + ump_bytes;
    
    // Send to all peers
    xSemaphoreTake(g_eth_state.peers_mutex, portMAX_DELAY);
    
    for (int i = 0; i < g_eth_state.num_active_peers; i++) {
        midi_ethernet_peer_t *peer = &g_eth_state.peers[i];
        
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(peer->port);
        inet_pton(AF_INET, peer->ip_addr, &dest_addr.sin_addr);
        
        sendto(g_eth_state.sock_fd, payload, payload_len, 0,
              (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        peer->packets_tx++;
        g_eth_state.stats.packets_tx_total++;
    }
    
    xSemaphoreGive(g_eth_state.peers_mutex);
    
    return ESP_OK;
}

// ... (additional helper functions similar to WiFi implementation)

bool midi_ethernet_is_link_up(void) {
    return g_eth_state.link_up;
}

esp_err_t midi_ethernet_get_stats(midi_ethernet_stats_t *stats) {
    if (!stats) return ESP_ERR_INVALID_ARG;
    *stats = g_eth_state.stats;
    stats->active_sessions = g_eth_state.num_active_peers;
    return ESP_OK;
}
