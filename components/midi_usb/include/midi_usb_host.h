/**
 * @file midi_usb_host.h
 * @brief USB MIDI Host Mode - Internal API
 * 
 * Implements USB Host mode using ESP-IDF USB Host library.
 * ESP32-S3 acts as a USB host when MIDI devices are connected.
 * 
 * Supports:
 * - USB-MIDI 1.0 devices (keyboards, controllers, etc.)
 * - USB-MIDI 2.0 devices (with UMP support)
 * - Multiple connected devices (limited by USB hub)
 * - Hot-plug detection (device connect/disconnect)
 * - Enumeration and configuration
 * 
 * Based on:
 * - USB Device Class Definition for MIDI Devices v1.0
 * - ESP-IDF USB Host Library
 */

#ifndef MIDI_USB_HOST_H
#define MIDI_USB_HOST_H

#include "midi_usb.h"
#include "esp_err.h"

/**
 * @brief Initialize USB Host mode
 * 
 * Initializes ESP-IDF USB Host library, registers MIDI class driver,
 * starts device monitoring task
 * 
 * @param config USB MIDI configuration
 * @return ESP_OK on success
 */
esp_err_t midi_usb_host_init(const midi_usb_config_t *config);

/**
 * @brief Deinitialize USB Host mode
 * 
 * Stops USB host, disconnects devices, releases resources
 * 
 * @return ESP_OK on success
 */
esp_err_t midi_usb_host_deinit(void);

/**
 * @brief Send packet in Host mode
 * 
 * Sends USB-MIDI packet to connected MIDI device via USB OUT endpoint
 * 
 * @param packet USB MIDI packet to send
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no device connected
 */
esp_err_t midi_usb_host_send_packet(const midi_usb_packet_t *packet);

/**
 * @brief Check if MIDI device is connected
 * 
 * @return true if MIDI device connected and ready
 */
bool midi_usb_host_is_device_connected(void);

/**
 * @brief Get connected device info
 * 
 * @param vid Output: Vendor ID
 * @param pid Output: Product ID
 * @param product_name Output: Product name string (buffer min 64 bytes)
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no device
 */
esp_err_t midi_usb_host_get_device_info(uint16_t *vid, uint16_t *pid, 
                                         char *product_name);

#endif /* MIDI_USB_HOST_H */
