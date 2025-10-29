/**
 * @file midi_usb_host.c
 * @brief USB MIDI Host Mode Implementation
 * 
 * Uses ESP-IDF USB Host library for host mode operation
 */

#include "midi_usb_host.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include <string.h>

static const char *TAG = "usb_host";

// USB Class codes for MIDI (Audio Class)
#define USB_CLASS_AUDIO             0x01
#define USB_SUBCLASS_AUDIOSTREAMING 0x03
#define USB_SUBCLASS_MIDISTREAMING  0x03

// USB-MIDI specific
#define USB_MIDI_CS_INTERFACE       0x24
#define USB_MIDI_MS_HEADER          0x01
#define USB_MIDI_IN_JACK            0x02
#define USB_MIDI_OUT_JACK           0x03

/**
 * @brief Connected MIDI device info
 */
typedef struct {
    usb_device_handle_t dev_hdl;
    uint8_t dev_addr;
    uint16_t vid;
    uint16_t pid;
    char product_name[64];
    
    uint8_t interface_num;
    uint8_t ep_in;          // Bulk IN endpoint (device → host)
    uint8_t ep_out;         // Bulk OUT endpoint (host → device)
    uint16_t ep_in_mps;     // Max packet size IN
    uint16_t ep_out_mps;    // Max packet size OUT
    
    bool configured;
    bool midi2_capable;
} midi_device_info_t;

/**
 * @brief USB Host state
 */
typedef struct {
    bool initialized;
    bool device_connected;
    midi_usb_config_t config;
    
    // Connected device
    midi_device_info_t device;
    
    // Callbacks
    midi_usb_rx_callback_t rx_callback;
    midi_usb_conn_callback_t conn_callback;
    void *callback_ctx;
    
    // Tasks
    TaskHandle_t host_task_handle;
    TaskHandle_t rx_task_handle;
    
    // Synchronization
    SemaphoreHandle_t device_mutex;
    
} midi_usb_host_state_t;

static midi_usb_host_state_t g_host_state = {0};

/**
 * @brief Parse USB-MIDI interface descriptor
 * 
 * Extracts endpoint addresses from MIDI streaming interface
 */
static esp_err_t parse_midi_interface(const usb_intf_desc_t *intf_desc,
                                       const usb_standard_desc_t *desc_start,
                                       size_t desc_len,
                                       midi_device_info_t *dev_info) {
    const uint8_t *ptr = (const uint8_t *)desc_start;
    const uint8_t *end = ptr + desc_len;
    
    dev_info->interface_num = intf_desc->bInterfaceNumber;
    
    ESP_LOGI(TAG, "Parsing MIDI interface %d", dev_info->interface_num);
    
    // Find endpoints
    while (ptr < end) {
        uint8_t desc_len = ptr[0];
        uint8_t desc_type = ptr[1];
        
        if (desc_len == 0) break;
        
        if (desc_type == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
            const usb_ep_desc_t *ep_desc = (const usb_ep_desc_t *)ptr;
            
            uint8_t ep_addr = ep_desc->bEndpointAddress;
            uint8_t ep_type = ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;
            
            // MIDI uses Bulk endpoints
            if (ep_type == USB_BM_ATTRIBUTES_XFER_BULK) {
                if (ep_addr & 0x80) {
                    // IN endpoint (device → host)
                    dev_info->ep_in = ep_addr;
                    dev_info->ep_in_mps = ep_desc->wMaxPacketSize;
                    ESP_LOGI(TAG, "  Bulk IN: 0x%02X (MPS: %d)", ep_addr, ep_desc->wMaxPacketSize);
                } else {
                    // OUT endpoint (host → device)
                    dev_info->ep_out = ep_addr;
                    dev_info->ep_out_mps = ep_desc->wMaxPacketSize;
                    ESP_LOGI(TAG, "  Bulk OUT: 0x%02X (MPS: %d)", ep_addr, ep_desc->wMaxPacketSize);
                }
            }
        }
        
        ptr += desc_len;
    }
    
    // Verify we found both endpoints
    if (dev_info->ep_in == 0 || dev_info->ep_out == 0) {
        ESP_LOGE(TAG, "Failed to find MIDI endpoints");
        return ESP_ERR_NOT_FOUND;
    }
    
    return ESP_OK;
}

/**
 * @brief Configure connected MIDI device
 */
static esp_err_t configure_midi_device(usb_device_handle_t dev_hdl,
                                        midi_device_info_t *dev_info) {
    esp_err_t err;
    
    // Get device descriptor
    const usb_device_desc_t *dev_desc;
    err = usb_host_get_device_descriptor(dev_hdl, &dev_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device descriptor: %s", esp_err_to_name(err));
        return err;
    }
    
    dev_info->vid = dev_desc->idVendor;
    dev_info->pid = dev_desc->idProduct;
    
    ESP_LOGI(TAG, "MIDI Device: VID=0x%04X, PID=0x%04X", dev_info->vid, dev_info->pid);
    
    // Get configuration descriptor
    const usb_config_desc_t *config_desc;
    err = usb_host_get_active_config_descriptor(dev_hdl, &config_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get config descriptor: %s", esp_err_to_name(err));
        return err;
    }
    
    // Find MIDI streaming interface
    int offset = 0;
    const usb_intf_desc_t *intf_desc = NULL;
    
    while ((intf_desc = usb_parse_interface_descriptor(config_desc, 
                                                         USB_CLASS_AUDIO,
                                                         USB_SUBCLASS_MIDISTREAMING,
                                                         -1, &offset)) != NULL) {
        ESP_LOGI(TAG, "Found MIDI Streaming interface");
        
        // Parse interface to extract endpoints
        err = parse_midi_interface(intf_desc, 
                                    (const usb_standard_desc_t *)config_desc,
                                    config_desc->wTotalLength,
                                    dev_info);
        if (err == ESP_OK) {
            break;  // Found and parsed successfully
        }
    }
    
    if (intf_desc == NULL) {
        ESP_LOGE(TAG, "No MIDI interface found");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Claim interface
    err = usb_host_interface_claim(g_host_state.device.dev_hdl,
                                    dev_info->interface_num,
                                    0);  // Alternate setting 0 (MIDI 1.0)
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to claim interface: %s", esp_err_to_name(err));
        return err;
    }
    
    dev_info->configured = true;
    ESP_LOGI(TAG, "MIDI device configured successfully");
    
    return ESP_OK;
}

/**
 * @brief USB RX task - reads from connected MIDI device
 */
static void midi_usb_host_rx_task(void *arg) {
    ESP_LOGI(TAG, "USB Host RX task started");
    
    uint8_t buffer[64];
    usb_transfer_t *transfer = NULL;
    
    while (1) {
        // Wait for device connection
        if (!g_host_state.device_connected) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // Allocate transfer if needed
        if (transfer == NULL) {
            esp_err_t err = usb_host_transfer_alloc(64, 0, &transfer);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to allocate transfer");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }
        
        // Setup Bulk IN transfer
        transfer->device_handle = g_host_state.device.dev_hdl;
        transfer->bEndpointAddress = g_host_state.device.ep_in;
        transfer->callback = NULL;  // Blocking transfer
        transfer->context = NULL;
        transfer->num_bytes = 64;
        transfer->timeout_ms = 100;
        
        // Submit transfer
        esp_err_t err = usb_host_transfer_submit_control(g_host_state.device.dev_hdl,
                                                          transfer);
        if (err == ESP_OK && transfer->actual_num_bytes > 0) {
            // Data received from MIDI device
            ESP_LOGD(TAG, "RX: %d bytes from MIDI device", transfer->actual_num_bytes);
            
            // Parse USB-MIDI packets (4 bytes each)
            for (int i = 0; i + 3 < transfer->actual_num_bytes; i += 4) {
                midi_usb_packet_t packet;
                
                uint8_t header = transfer->data_buffer[i];
                uint8_t cin = header & 0x0F;
                
                if (cin == 0) continue;  // Skip padding
                
                packet.cable_number = (header >> 4) & 0x0F;
                packet.protocol = MIDI_USB_PROTOCOL_1_0;
                packet.timestamp_us = esp_timer_get_time();
                packet.data.midi1.cin = cin;
                packet.data.midi1.midi_bytes[0] = transfer->data_buffer[i + 1];
                packet.data.midi1.midi_bytes[1] = transfer->data_buffer[i + 2];
                packet.data.midi1.midi_bytes[2] = transfer->data_buffer[i + 3];
                
                // Call user callback
                if (g_host_state.rx_callback) {
                    g_host_state.rx_callback(&packet, g_host_state.callback_ctx);
                }
            }
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "RX transfer failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    // Cleanup (never reached in normal operation)
    if (transfer) {
        usb_host_transfer_free(transfer);
    }
}

/**
 * @brief USB Host event callback
 */
static void usb_host_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg) {
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "New USB device detected (addr: %d)", event_msg->new_dev.address);
            
            // Open device
            usb_device_handle_t dev_hdl;
            esp_err_t err = usb_host_device_open(g_host_state.device.dev_hdl,
                                                  event_msg->new_dev.address,
                                                  &dev_hdl);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open device: %s", esp_err_to_name(err));
                break;
            }
            
            // Store device info
            g_host_state.device.dev_hdl = dev_hdl;
            g_host_state.device.dev_addr = event_msg->new_dev.address;
            
            // Configure MIDI device
            err = configure_midi_device(dev_hdl, &g_host_state.device);
            if (err == ESP_OK) {
                g_host_state.device_connected = true;
                ESP_LOGI(TAG, "MIDI device ready");
                
                // Call connection callback
                if (g_host_state.conn_callback) {
                    g_host_state.conn_callback(true, g_host_state.callback_ctx);
                }
            } else {
                ESP_LOGE(TAG, "Failed to configure MIDI device");
                usb_host_device_close(g_host_state.device.dev_hdl, dev_hdl);
            }
            break;
            
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGI(TAG, "USB device disconnected (addr: %d)", event_msg->dev_gone.dev_addr);
            
            if (g_host_state.device_connected &&
                g_host_state.device.dev_addr == event_msg->dev_gone.dev_addr) {
                
                // Release interface
                if (g_host_state.device.configured) {
                    usb_host_interface_release(g_host_state.device.dev_hdl,
                                                g_host_state.device.interface_num);
                }
                
                // Close device
                usb_host_device_close(g_host_state.device.dev_hdl, 
                                       g_host_state.device.dev_hdl);
                
                // Clear state
                memset(&g_host_state.device, 0, sizeof(midi_device_info_t));
                g_host_state.device_connected = false;
                
                ESP_LOGI(TAG, "MIDI device removed");
                
                // Call disconnection callback
                if (g_host_state.conn_callback) {
                    g_host_state.conn_callback(false, g_host_state.callback_ctx);
                }
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief USB Host task - handles USB host library events
 */
static void usb_host_task(void *arg) {
    ESP_LOGI(TAG, "USB Host task started");
    
    while (1) {
        // Handle USB host events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        // Process events if needed
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGW(TAG, "No USB host clients");
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "All USB devices freed");
        }
    }
}

/**
 * @brief Initialize USB Host mode
 */
esp_err_t midi_usb_host_init(const midi_usb_config_t *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_host_state.initialized) {
        ESP_LOGW(TAG, "USB host already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing USB Host mode");
    
    // Clear state
    memset(&g_host_state, 0, sizeof(g_host_state));
    g_host_state.config = *config;
    g_host_state.rx_callback = config->rx_callback;
    g_host_state.conn_callback = config->conn_callback;
    g_host_state.callback_ctx = config->callback_ctx;
    
    // Create mutex
    g_host_state.device_mutex = xSemaphoreCreateMutex();
    if (!g_host_state.device_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    // Install USB host library
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    
    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB host install failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(g_host_state.device_mutex);
        return err;
    }
    
    // Register client
    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = usb_host_event_callback,
            .callback_arg = NULL
        }
    };
    
    usb_host_client_handle_t client_hdl;
    err = usb_host_client_register(&client_config, &client_hdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Client register failed: %s", esp_err_to_name(err));
        usb_host_uninstall();
        vSemaphoreDelete(g_host_state.device_mutex);
        return err;
    }
    
    // Create USB host task
    BaseType_t task_created = xTaskCreatePinnedToCore(
        usb_host_task,
        "usb_host",
        4096,
        NULL,
        5,
        &g_host_state.host_task_handle,
        0  // Core 0
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create host task");
        usb_host_client_unregister(client_hdl);
        usb_host_uninstall();
        vSemaphoreDelete(g_host_state.device_mutex);
        return ESP_FAIL;
    }
    
    // Create RX task
    task_created = xTaskCreatePinnedToCore(
        midi_usb_host_rx_task,
        "midi_host_rx",
        4096,
        NULL,
        10,  // High priority
        &g_host_state.rx_task_handle,
        1    // Core 1
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        vTaskDelete(g_host_state.host_task_handle);
        usb_host_client_unregister(client_hdl);
        usb_host_uninstall();
        vSemaphoreDelete(g_host_state.device_mutex);
        return ESP_FAIL;
    }
    
    g_host_state.initialized = true;
    
    ESP_LOGI(TAG, "USB Host mode initialized");
    ESP_LOGI(TAG, "Waiting for MIDI device connection...");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize USB Host mode
 */
esp_err_t midi_usb_host_deinit(void) {
    if (!g_host_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing USB Host mode");
    
    // Delete tasks
    if (g_host_state.rx_task_handle) {
        vTaskDelete(g_host_state.rx_task_handle);
    }
    if (g_host_state.host_task_handle) {
        vTaskDelete(g_host_state.host_task_handle);
    }
    
    // Uninstall USB host
    usb_host_uninstall();
    
    // Delete mutex
    if (g_host_state.device_mutex) {
        vSemaphoreDelete(g_host_state.device_mutex);
    }
    
    g_host_state.initialized = false;
    
    return ESP_OK;
}

/**
 * @brief Send packet in Host mode
 */
esp_err_t midi_usb_host_send_packet(const midi_usb_packet_t *packet) {
    if (!g_host_state.initialized || !g_host_state.device_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Build USB-MIDI packet (4 bytes)
    uint8_t usb_packet[4] = {
        (packet->cable_number << 4) | packet->data.midi1.cin,
        packet->data.midi1.midi_bytes[0],
        packet->data.midi1.midi_bytes[1],
        packet->data.midi1.midi_bytes[2]
    };
    
    // Allocate transfer
    usb_transfer_t *transfer;
    esp_err_t err = usb_host_transfer_alloc(4, 0, &transfer);
    if (err != ESP_OK) {
        return err;
    }
    
    // Setup transfer
    transfer->device_handle = g_host_state.device.dev_hdl;
    transfer->bEndpointAddress = g_host_state.device.ep_out;
    transfer->callback = NULL;  // Blocking
    transfer->context = NULL;
    transfer->num_bytes = 4;
    transfer->timeout_ms = 100;
    memcpy(transfer->data_buffer, usb_packet, 4);
    
    // Submit transfer
    err = usb_host_transfer_submit_control(g_host_state.device.dev_hdl, transfer);
    
    // Free transfer
    usb_host_transfer_free(transfer);
    
    return err;
}

/**
 * @brief Check if MIDI device is connected
 */
bool midi_usb_host_is_device_connected(void) {
    return g_host_state.device_connected;
}

/**
 * @brief Get connected device info
 */
esp_err_t midi_usb_host_get_device_info(uint16_t *vid, uint16_t *pid,
                                         char *product_name) {
    if (!g_host_state.device_connected) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (vid) *vid = g_host_state.device.vid;
    if (pid) *pid = g_host_state.device.pid;
    if (product_name) {
        strncpy(product_name, g_host_state.device.product_name, 63);
        product_name[63] = '\0';
    }
    
    return ESP_OK;
}
