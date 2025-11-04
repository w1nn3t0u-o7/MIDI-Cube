#include "midi_wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi_transport";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static midi_wifi_config_t midi_wifi_config = {
    .is_connected = false,
    .retry_num = 0,
    .max_retry = 10,
};

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        midi_wifi_config.is_connected = false;
        
        if (midi_wifi_config.max_retry == 0 || midi_wifi_config.retry_num < midi_wifi_config.max_retry) {
            esp_wifi_connect();
            midi_wifi_config.retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP (attempt %d)", midi_wifi_config.retry_num);
        } else {
            xEventGroupSetBits(midi_wifi_config.wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to AP after %d attempts", midi_wifi_config.max_retry);
        }
        
        if (midi_wifi_config.disconnected_cb) {
            midi_wifi_config.disconnected_cb();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        midi_wifi_config.retry_num = 0;
        midi_wifi_config.is_connected = true;
        
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "Got IP address: %s", ip_str);
        
        xEventGroupSetBits(midi_wifi_config.wifi_event_group, WIFI_CONNECTED_BIT);
        
        if (midi_wifi_config.connected_cb) {
            midi_wifi_config.connected_cb(midi_wifi_config.wifi_netif, ip_str);
        }
    }
}

esp_err_t midi_wifi_init(void)
{
    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    // Create event group
    midi_wifi_config.wifi_event_group = xEventGroupCreate();
    if (midi_wifi_config.wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop if not already created
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    
    // Create default WiFi station interface
    midi_wifi_config.wifi_netif = esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = MIDI_WIFI_SSID,
            .password = MIDI_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization finished. Connecting to SSID: %s", MIDI_WIFI_SSID);
    
    return ESP_OK;
}

void midi_wifi_register_callbacks(wifi_connected_cb_t connected_cb,
                                       wifi_disconnected_cb_t disconnected_cb)
{
    midi_wifi_config.connected_cb = connected_cb;
    midi_wifi_config.disconnected_cb = disconnected_cb;
}

esp_netif_t* midi_wifi_get_netif(void)
{
    return midi_wifi_config.wifi_netif;
}

bool midi_wifi_is_connected(void)
{
    return midi_wifi_config.is_connected;
}

esp_err_t midi_wifi_deinit(void)
{
    if (midi_wifi_config.wifi_netif) {
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_deinit());
        esp_netif_destroy(midi_wifi_config.wifi_netif);
        midi_wifi_config.wifi_netif = NULL;
    }
    
    if (midi_wifi_config.wifi_event_group) {
        vEventGroupDelete(midi_wifi_config.wifi_event_group);
        midi_wifi_config.wifi_event_group = NULL;
    }
    
    return ESP_OK;
}
