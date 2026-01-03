/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "sdkconfig.h"

#include "midi_ethernet.h"
static const char *TAG = "midi_eth";

static midi_eth_config_t midi_eth_config = {
    .is_connected = false,
};

/* Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "MASK: " IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "GW: " IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

#ifdef CONFIG_MIDI_ETH_IP_STATIC
    ESP_LOGI(TAG, "Ethernet running in STATIC IP mode (direct connection)");
#else
    ESP_LOGI(TAG, "Ethernet running in DHCP mode (router connection)");
#endif
    midi_eth_config.is_connected = true;

    if (midi_eth_config.connected_cb) {
        char ip_str[16];
        esp_ip4addr_ntoa(&ip_info->ip, ip_str, sizeof(ip_str));
        midi_eth_config.connected_cb(midi_eth_config.eth_netif, ip_str);
    }
}

void midi_eth_register_callbacks(eth_connected_cb_t connected_cb,
                                          eth_disconnected_cb_t disconnected_cb)
{
    midi_eth_config.connected_cb = connected_cb;
    midi_eth_config.disconnected_cb = disconnected_cb;
}

esp_err_t midi_eth_init(void)
{
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    char if_key_str[10];
    char if_desc_str[10];

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Initialize Ethernet driver
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    if (eth_port_cnt == 0) {
        ESP_LOGE(TAG, "No Ethernet interface found!");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Ethernet port count: %d", eth_port_cnt);

    // Create instance(s) of esp-netif for Ethernet(s)
    if (eth_port_cnt == 1) {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        midi_eth_config.eth_netif = esp_netif_new(&cfg);
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(midi_eth_config.eth_netif, esp_eth_new_netif_glue(eth_handles[0])));

#ifdef CONFIG_MIDI_ETH_IP_STATIC
        // Configure STATIC IP
        ESP_LOGI(TAG, "Configuring static IP: " CONFIG_MIDI_ETH_STATIC_IP);
        
        // Stop DHCP client first
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(midi_eth_config.eth_netif));
        
        // Parse and set static IP configuration
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
        
        ip_info.ip.addr = esp_ip4addr_aton(CONFIG_MIDI_ETH_STATIC_IP);
        ip_info.netmask.addr = esp_ip4addr_aton(CONFIG_MIDI_ETH_STATIC_NETMASK);
        ip_info.gw.addr = esp_ip4addr_aton(CONFIG_MIDI_ETH_STATIC_GATEWAY);
        
        ESP_ERROR_CHECK(esp_netif_set_ip_info(midi_eth_config.eth_netif, &ip_info));
        
        ESP_LOGI(TAG, "Static IP configured successfully");
        ESP_LOGI(TAG, "  IP: " CONFIG_MIDI_ETH_STATIC_IP);
        ESP_LOGI(TAG, "  Netmask: " CONFIG_MIDI_ETH_STATIC_NETMASK);
        ESP_LOGI(TAG, "  Gateway: " CONFIG_MIDI_ETH_STATIC_GATEWAY);
#else
        // DHCP mode - already enabled by default
        ESP_LOGI(TAG, "Using DHCP - waiting for IP address from router...");
#endif

    } else {
        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
        // esp-netif configuration parameters for each interface (name, priority, etc.).
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };

        for (int i = 0; i < eth_port_cnt; i++) {
            sprintf(if_key_str, "ETH_%d", i);
            sprintf(if_desc_str, "eth%d", i);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i * 5;
            esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
        }
    }

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

    // Print each device info
    for (int i = 0; i < eth_port_cnt; i++) {
        eth_dev_info_t info = ethernet_init_get_dev_info(eth_handles[i]);
        if (info.type == ETH_DEV_TYPE_INTERNAL_ETH) {
            ESP_LOGI(TAG, "Device Name: %s", info.name);
            ESP_LOGI(TAG, "Device type: ETH_DEV_TYPE_INTERNAL_ETH(%d)", info.type);
            ESP_LOGI(TAG, "Pins: mdc: %d, mdio: %d", info.pin.eth_internal_mdc, info.pin.eth_internal_mdio);
        } else if (info.type == ETH_DEV_TYPE_SPI) {
            ESP_LOGI(TAG, "Device Name: %s", info.name);
            ESP_LOGI(TAG, "Device type: ETH_DEV_TYPE_SPI(%d)", info.type);
            ESP_LOGI(TAG, "Pins: cs: %d, intr: %d", info.pin.eth_spi_cs, info.pin.eth_spi_int);
        }
    }

    return ESP_OK;
}
