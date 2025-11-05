#include "midi_ethernet.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_mac.h"
#include "esp_eth_mac_w5500.h"
#include "esp_eth_phy_w5500.h"
#include "esp_event.h"

static const char *TAG = "midi_eth";

static midi_eth_config_t midi_eth_config = {
    .spi_host = SPI2_HOST,
    .spi_clock_mhz = SPI_MASTER_FREQ_20M,
    .gpio_mosi = 11,
    .gpio_miso = 13,
    .gpio_sclk = 12,
    .gpio_cs = 10,
    .gpio_int = 9,
    .gpio_rst = 3,
};

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == ETH_EVENT) {
        switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Up");
            midi_eth_config.link_up = true;
            break;
            
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Ethernet Link Down");
            midi_eth_config.link_up = false;
            midi_eth_config.is_connected = false;
            
            if (midi_eth_config.disconnected_cb) {
                midi_eth_config.disconnected_cb();
            }
            break;
            
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
            
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            midi_eth_config.is_connected = false;
            midi_eth_config.link_up = false;
            break;
            
        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        midi_eth_config.is_connected = true;
        
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "Got IP address: %s", ip_str);
        
        if (midi_eth_config.connected_cb) {
            midi_eth_config.connected_cb(midi_eth_config.eth_netif, ip_str);
        }
    }
}

esp_err_t midi_eth_init(void)
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

    // CRITICAL: Install GPIO ISR service BEFORE W5500 initialization
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create network interface for Ethernet
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    midi_eth_config.eth_netif = esp_netif_new(&netif_cfg);
    
    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = midi_eth_config.gpio_mosi,
        .miso_io_num = midi_eth_config.gpio_miso,
        .sclk_io_num = midi_eth_config.gpio_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    ESP_LOGI(TAG, "Initializing SPI bus on host %d", midi_eth_config.spi_host);
    ESP_ERROR_CHECK(spi_bus_initialize(midi_eth_config.spi_host, &buscfg, SPI_DMA_CH_AUTO));
    
    // Configure SPI device for W5500
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = midi_eth_config.spi_clock_mhz,
        .queue_size = 20,
        .spics_io_num = midi_eth_config.gpio_cs,
    };
    
    // W5500 specific configuration
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(midi_eth_config.spi_host, &devcfg);
    w5500_config.int_gpio_num = midi_eth_config.gpio_int;
    
    // Create W5500 MAC
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    
    // Create W5500 PHY
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = midi_eth_config.gpio_rst;
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    
    // Install Ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &midi_eth_config.eth_handle));
    
    // Attach Ethernet driver to TCP/IP stack
    void *glue = esp_eth_new_netif_glue(midi_eth_config.eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(midi_eth_config.eth_netif, glue));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                &eth_event_handler, NULL));
    
    // Start Ethernet
    ESP_ERROR_CHECK(esp_eth_start(midi_eth_config.eth_handle));

    // Set MAC address for W5500 (it has no factory MAC)
    uint8_t base_mac[6];
    esp_read_mac(base_mac, ESP_MAC_ETH);  // Get ESP32's base MAC + 3

    ESP_LOGI(TAG, "Setting W5500 MAC: %02X:%02X:%02X:%02X:%02X:%02X",
            base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);

    // Set MAC on both hardware and esp-netif layer
    ESP_ERROR_CHECK(esp_netif_set_mac(midi_eth_config.eth_netif, base_mac));
    
    ESP_LOGI(TAG, "W5500 Ethernet initialized successfully");
    
    return ESP_OK;
}

void midi_eth_register_callbacks(eth_connected_cb_t connected_cb,
                                          eth_disconnected_cb_t disconnected_cb)
{
    midi_eth_config.connected_cb = connected_cb;
    midi_eth_config.disconnected_cb = disconnected_cb;
}

esp_netif_t* midi_eth_get_netif(void)
{
    return midi_eth_config.eth_netif;
}

bool midi_eth_is_connected(void)
{
    return midi_eth_config.is_connected && midi_eth_config.link_up;
}

esp_err_t midi_eth_deinit(void)
{
    if (midi_eth_config.eth_handle) {
        ESP_ERROR_CHECK(esp_eth_stop(midi_eth_config.eth_handle));
        ESP_ERROR_CHECK(esp_eth_driver_uninstall(midi_eth_config.eth_handle));
        midi_eth_config.eth_handle = NULL;
    }
    
    if (midi_eth_config.eth_netif) {
        esp_netif_destroy(midi_eth_config.eth_netif);
        midi_eth_config.eth_netif = NULL;
    }
    
    return ESP_OK;
}
