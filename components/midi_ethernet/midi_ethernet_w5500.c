/**
 * @file midi_ethernet_w5500.c
 * @brief W5500 Hardware Initialization and Control
 * 
 * Handles SPI communication with W5500 Ethernet controller
 */

#include "midi_ethernet.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_eth_netif_glue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "eth_w5500";

// W5500 chip configuration
#define W5500_SPI_MAX_TRANSFER_SIZE  1024

/**
 * @brief Initialize SPI bus for W5500
 */
esp_err_t midi_ethernet_w5500_init_spi(const midi_ethernet_config_t *config,
                                        spi_device_handle_t *spi_handle) {
    ESP_LOGI(TAG, "Initializing SPI for W5500");
    ESP_LOGI(TAG, "  Host: SPI%d", config->spi_host);
    ESP_LOGI(TAG, "  Clock: %d MHz", config->spi_clock_speed_mhz);
    ESP_LOGI(TAG, "  SCLK: GPIO%d", config->gpio_sclk);
    ESP_LOGI(TAG, "  MOSI: GPIO%d", config->gpio_mosi);
    ESP_LOGI(TAG, "  MISO: GPIO%d", config->gpio_miso);
    ESP_LOGI(TAG, "  CS:   GPIO%d", config->gpio_cs);
    ESP_LOGI(TAG, "  INT:  GPIO%d", config->gpio_int);
    
    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->gpio_mosi,
        .miso_io_num = config->gpio_miso,
        .sclk_io_num = config->gpio_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = W5500_SPI_MAX_TRANSFER_SIZE,
    };
    
    esp_err_t ret = spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure SPI device (W5500)
    spi_device_interface_config_t devcfg = {
        .command_bits = 16,              // W5500 uses 16-bit command
        .address_bits = 8,               // W5500 uses 8-bit address
        .mode = 0,                       // SPI mode 0
        .clock_speed_hz = config->spi_clock_speed_mhz * 1000 * 1000,
        .spics_io_num = config->gpio_cs,
        .queue_size = 20,
        .cs_ena_pretrans = 2,            // W5500 CS setup time
        .cs_ena_posttrans = 2,           // W5500 CS hold time
    };
    
    ret = spi_bus_add_device(config->spi_host, &devcfg, spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "SPI initialized successfully");
    return ESP_OK;
}

/**
 * @brief Initialize W5500 Ethernet MAC/PHY[web:279][web:281]
 */
esp_err_t midi_ethernet_w5500_init_driver(const midi_ethernet_config_t *config,
                                           esp_eth_handle_t *eth_handle,
                                           esp_netif_t **netif) {
    ESP_LOGI(TAG, "Initializing W5500 Ethernet driver");
    
    // Create network interface
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    *netif = esp_netif_new(&netif_cfg);
    
    // Configure W5500 MAC
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(config->spi_host, NULL);
    w5500_config.int_gpio_num = config->gpio_int;
    
    // Create MAC instance[web:281]
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.smi_mdc_gpio_num = -1;   // W5500 doesn't use MDC/MDIO
    mac_config.smi_mdio_gpio_num = -1;
    
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to create W5500 MAC");
        return ESP_FAIL;
    }
    
    // Create PHY instance[web:281]
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;  // W5500 doesn't need external PHY reset
    
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG, "Failed to create W5500 PHY");
        return ESP_FAIL;
    }
    
    // Install Ethernet driver[web:279]
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_err_t ret = esp_eth_driver_install(&eth_config, eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Attach driver to network interface
    void *glue = esp_eth_new_netif_glue(*eth_handle);
    ret = esp_netif_attach(*netif, glue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Netif attach failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "W5500 driver initialized");
    return ESP_OK;
}

/**
 * @brief Configure static IP or DHCP[web:276][web:277]
 */
esp_err_t midi_ethernet_w5500_configure_ip(esp_netif_t *netif,
                                            const midi_ethernet_config_t *config) {
    if (config->use_dhcp) {
        ESP_LOGI(TAG, "Using DHCP for IP configuration");
        esp_netif_dhcpc_start(netif);
    } else {
        ESP_LOGI(TAG, "Using static IP: %s", config->static_ip);
        
        // Stop DHCP client
        esp_netif_dhcpc_stop(netif);
        
        // Configure static IP
        esp_netif_ip_info_t ip_info;
        ip_info.ip.addr = esp_ip4addr_aton(config->static_ip);
        ip_info.netmask.addr = esp_ip4addr_aton(config->netmask);
        ip_info.gw.addr = esp_ip4addr_aton(config->gateway);
        
        esp_err_t ret = esp_netif_set_ip_info(netif, &ip_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set IP info failed: %s", esp_err_to_name(ret));
            return ret;
        }
        
        ESP_LOGI(TAG, "  IP:      %s", config->static_ip);
        ESP_LOGI(TAG, "  Netmask: %s", config->netmask);
        ESP_LOGI(TAG, "  Gateway: %s", config->gateway);
    }
    
    return ESP_OK;
}
