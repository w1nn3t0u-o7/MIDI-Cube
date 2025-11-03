#include "midi_ethernet.h"
#include "esp_eth.h"
#include "esp_eth_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_eth_mac_w5500.h"
#include "esp_eth_phy_w5500.h"
#include "esp_event.h"

static const char *TAG = "eth_transport";

static esp_netif_t *s_eth_netif = NULL;
static esp_eth_handle_t s_eth_handle = NULL;
static eth_connected_cb_t s_connected_cb = NULL;
static eth_disconnected_cb_t s_disconnected_cb = NULL;
static bool s_is_connected = false;
static bool s_link_up = false;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == ETH_EVENT) {
        switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Up");
            s_link_up = true;
            break;
            
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Ethernet Link Down");
            s_link_up = false;
            s_is_connected = false;
            
            if (s_disconnected_cb) {
                s_disconnected_cb();
            }
            break;
            
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
            
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            s_is_connected = false;
            s_link_up = false;
            break;
            
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_is_connected = true;
        
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "Got IP address: %s", ip_str);
        
        if (s_connected_cb) {
            s_connected_cb(s_eth_netif, ip_str);
        }
    }
}

esp_err_t ethernet_transport_init(const ethernet_transport_config_t *config)
{
    esp_err_t ret;
    
    // Initialize TCP/IP stack if not already done
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    
    // Create default event loop if not already created
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    
    // Create network interface for Ethernet
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    
    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->gpio_mosi,
        .miso_io_num = config->gpio_miso,
        .sclk_io_num = config->gpio_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    ESP_LOGI(TAG, "Initializing SPI bus on host %d", config->spi_host);
    ESP_ERROR_CHECK(spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO));
    
    // Configure SPI device for W5500
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = config->spi_clock_mhz * 1000000,
        .queue_size = 20,
        .spics_io_num = config->gpio_cs,
    };
    
    // W5500 specific configuration
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(config->spi_host, &devcfg);
    w5500_config.int_gpio_num = config->gpio_int;
    
    // Create W5500 MAC
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    
    // Create W5500 PHY
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = config->gpio_rst;
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    
    // Install Ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &s_eth_handle));
    
    // Attach Ethernet driver to TCP/IP stack
    void *glue = esp_eth_new_netif_glue(s_eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, glue));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                &eth_event_handler, NULL));
    
    // Start Ethernet
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
    
    ESP_LOGI(TAG, "W5500 Ethernet initialized successfully");
    
    return ESP_OK;
}

void ethernet_transport_register_callbacks(eth_connected_cb_t connected_cb,
                                          eth_disconnected_cb_t disconnected_cb)
{
    s_connected_cb = connected_cb;
    s_disconnected_cb = disconnected_cb;
}

esp_netif_t* ethernet_transport_get_netif(void)
{
    return s_eth_netif;
}

bool ethernet_transport_is_connected(void)
{
    return s_is_connected && s_link_up;
}

esp_err_t ethernet_transport_deinit(void)
{
    if (s_eth_handle) {
        ESP_ERROR_CHECK(esp_eth_stop(s_eth_handle));
        ESP_ERROR_CHECK(esp_eth_driver_uninstall(s_eth_handle));
        s_eth_handle = NULL;
    }
    
    if (s_eth_netif) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
    }
    
    return ESP_OK;
}
